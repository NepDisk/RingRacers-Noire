#!/usr/bin/python
# A homebrew ZIP file writer by GenericHeroGuy

import sys
from struct import Struct
from glob import glob
import zlib
from asyncio.subprocess import PIPE
import os
import asyncio

PkLocalHeader = Struct("<"
	"I" # magic
	"H" # min version
	"H" # flags
	"H" # method
	"H" # file time
	"H" # file date
	"I" # CRC32
	"I" # compressed size
	"I" # uncompressed size
	"H" # length of filename
	"H" # length of extra
)

PkCentralHeader = Struct("<"
	"I" # magic
	"H" # created version
	"H" # min version
	"H" # flags
	"H" # method
	"H" # file time
	"H" # file date
	"I" # CRC32
	"I" # compressed size
	"I" # uncompressed size
	"H" # length of filename
	"H" # length of extra
	"H" # comment length
	"H" # disk number
	"H" # internal attributes
	"I" # external attributes
	"I" # offset to local header
)

PkCentralEnd = Struct("<"
	"I" # magic
	"H" # disk number
	"H" # disk number of central directory
	"H" # number of entries on this disk
	"H" # number of entries total
	"I" # size of central directory
	"I" # offset to central directory
	"H" # comment length
)

MINVERSION = 20
CREATEVERSION = 30
CENTRALMAGIC = 0x02014b50
LOCALMAGIC = 0x04034b50
ENDMAGIC = 0x06054b50

DEFAULTOUTPUT = "output.pk3"

class PkFile:
	def __repr__(self):
		return self.name

async def Deflate(filename, zipname, level, i, usezlib):
	f = open(filename, "rb")
	file = PkFile()
	file.crc = zlib.crc32(f.read())
	file.size = f.seek(0, 1)

	if usezlib:
		f.seek(0, 0)
		obj = zlib.compressobj(level = 9, wbits = -15)
		file.data = obj.compress(f.read())
		file.data += obj.flush()
		f.close()
	else: # zopfli
		f.close()
		# zopfli does not accept input from stdin
		p = await asyncio.create_subprocess_shell(f'zopfli -c --deflate --i{level} "{filename}"', stdout=PIPE, stderr=PIPE)
		file.data, stderr = await p.communicate()
		# zopfli doesn't set exit code on error
		if stderr:
			raise ValueError("Zopfli error: " + stderr.decode())

	file.name = zipname
	file.extra = b""
	file.method = 8
	file.modtime = 0
	file.moddate = 0x0021
	file.flags = 2
	file.comment = ""
	file.compressedsize = len(file.data)
	return file, i

async def DoFiles(files, base, level, threads, usezlib):
	out = {}
	tasklist = [] # weak references...
	sem = asyncio.BoundedSemaphore(threads)

	def Done(task):
		try:
			res = task.result()
			if res:
				file, i = res
				out[i] = file
		except asyncio.exceptions.CancelledError:
			raise ValueError("Concurrency failure")
		tasklist.remove(task)
		sem.release()

	i = 0
	total = len(files)
	for name, matchcount in files.items():
		i += 1
		if matchcount <= 0:
			continue
		zipname = name.removeprefix(base).removeprefix("/")
		print(end="\033[2K")
		print(f"[{i}/{total}]", zipname, end="\r")

		await sem.acquire()
		task = asyncio.create_task(Deflate(name, zipname, level, i, usezlib))
		task.add_done_callback(Done)
		tasklist.append(task)

	# wait for all running tasks to finish
	free = 0
	while free < threads:
		await sem.acquire()
		free += 1

	return out

# six levels of indentation, GO!
def FindCentralEnd(f):
	while a := f.read(1):
		if a == b"P":
			if f.read(1) == b"K":
				if f.read(1) == b"\x05":
					if f.read(1) == b"\x06":
						return
	raise ValueError("No central directory found")

def SetBasePath(args, new):
	if not new.endswith("/"):
		new += "/"
	args.basepath = new

async def main(args):
	try:
		if args.listfile == "-":
			listfile = sys.stdin
		else:
			listfile = open(args.listfile, "r")
	except (FileNotFoundError, TypeError):
		print("No list file", f"named {args.listfile} was found." if args.listfile else "specified.")
		print("Try './pk3make.py build/sglua.txt'")
		sys.exit()

	# detect zopfli?
	if not args.zlib:
		# just run it and see if it works
		p = await asyncio.create_subprocess_shell("zopfli -h", stdout=PIPE, stderr=PIPE)
		await p.wait()
		if p.returncode != 0:
			print("WARNING: Zopfli not found, falling back to zlib")
			args.zlib = True

	infiles = {}
	if args.basepath:
		# ensure base path is correctly formatted
		SetBasePath(args, args.basepath)

	for line in listfile.readlines():
		line = line.strip()
		if not line or line.startswith("#"):
			continue

		if line.startswith("$"):
			try:
				var, value = line[1:].split()
			except ValueError:
				sys.exit("Syntax error: " + line)

			if var == "basepath":
				if not args.basepath:
					SetBasePath(args, value)
					print("Setting base to", args.basepath)
				continue
			if var == "output":
				if args.output == DEFAULTOUTPUT:
					args.output = value
					print("Setting output to", value)
				continue

			sys.exit("Unknown variable: " + var)

		found = False
		for name in sorted(glob(args.basepath + line, recursive=True)):
			found = True
			if not os.path.isdir(name):
				infiles[name] = infiles.get(name, 0) + 1
		if not found:
			sys.exit("File not found: " + args.basepath + line)

	listfile.close()

	if not infiles:
		sys.exit("This list file is empty!")

	if dirs := os.path.dirname(args.output):
		os.makedirs(dirs, exist_ok=True)

	if not args.comment:
		p = await asyncio.create_subprocess_shell("git rev-parse --short --verify HEAD", stdout=PIPE, stderr=PIPE)
		stdout, stderr = await p.communicate()
		args.comment = stdout.decode().strip() if not stderr else "unknown"

	# maintain order of files, according to input
	indices = {}
	i = 0
	for name in infiles:
		i += 1
		indices[name] = i

	# pass through files from the output archive
	passthru = {}
	lastmod = None
	try:
		lastmod = os.path.getmtime(args.output)
	except:
		pass

	orderchanged = False

	if lastmod and not args.force:
		f = open(args.output, "rb")

		# seek to central directory
		# try and make this a bit faster...
		try:
			f.seek(-PkCentralEnd.size, 2)
			FindCentralEnd(f)
		except ValueError:
			print("Finding central directory...")
			f.seek(0)
			FindCentralEnd(f)

		f.seek(-4, 1)
		entries, clen, cofs = PkCentralEnd.unpack(f.read(PkCentralEnd.size))[4:7]
		f.seek(cofs)

		for _ in range(entries):
			central = PkCentralHeader.unpack(f.read(PkCentralHeader.size))
			pa = PkFile()
			magic, _, _, pa.flags, pa.method, pa.modtime, pa.moddate, pa.crc, pa.compressedsize, pa.size, namelen, extralen, commentlen, _, _, _, localofs = central
			if magic != CENTRALMAGIC:
				raise ValueError("Corrupted central directory")
			pa.name = f.read(namelen).decode("ascii")
			pa.extra = f.read(extralen)
			pa.comment = f.read(commentlen).decode("ascii")
			old = f.tell()

			try:
				if lastmod < os.path.getmtime(args.basepath + pa.name):
					print("Updating", pa.name)
				else:
					f.seek(localofs)
					local = PkLocalHeader.unpack(f.read(PkLocalHeader.size))
					magic, _, _, _, _, _, _, _, _, namelen, extralen, = local
					if magic != LOCALMAGIC:
						raise ValueError("Corrupted local header")
					f.read(namelen + extralen)
					pa.data = f.read(pa.compressedsize)
					f.seek(old)

					try:
						#print("Passing through", pa.name)
						passthru[indices[args.basepath + pa.name]] = pa
						if infiles[args.basepath + pa.name]:
							infiles[args.basepath + pa.name] = 0
					except KeyError:
						# SLADE adding directory entries most likely, don't bother yelling
						pass#print(pa.name, "does not exist in input files")
			except FileNotFoundError:
				print("Unknown file", pa.name, "in archive. File will be deleted!")
				orderchanged = True
		f.close()

	# check if the input order changed
	for i, pa in zip(infiles.keys(), passthru.values()):
		if i != args.basepath + pa.name:
			orderchanged = True
			break

	# compress all the input files
	output = await asyncio.create_task(DoFiles(infiles, args.basepath, args.level, int(args.jobs), args.zlib))
	if not output and not orderchanged:
		sys.exit("Nothing to do!")

	print("\033[2KWriting", args.output)
	out = open(args.output, "wb")

	output.update(passthru)
	outfiles = [x for _, x in sorted(output.items())]
	offsets = {}
	strip = args.strip

	for file in outfiles:
		local = PkLocalHeader.pack(LOCALMAGIC, MINVERSION, file.flags, file.method, file.modtime, file.moddate, file.crc,
		                           file.compressedsize, file.size, 0 if strip else len(file.name), len(file.extra))
		offsets[file] = out.tell()
		out.write(local)
		if not strip:
			out.write(file.name.encode("ascii"))
		out.write(file.data)

	centralstart = out.tell()

	for file in outfiles:
		central = PkCentralHeader.pack(CENTRALMAGIC, CREATEVERSION, MINVERSION, file.flags, file.method, file.modtime,
		                               file.moddate, file.crc, file.compressedsize, file.size, len(file.name),
		                               len(file.extra), len(args.comment or file.comment), 0, 0, 0, offsets[file])
		out.write(central)
		out.write(file.name.encode("ascii"))
		# Kart doesn't count the bytes used by the archive comment and complains
		# so just tag the first file in the archive
		out.write((args.comment or file.comment).encode("ascii"))
		args.comment = ""

	end = PkCentralEnd.pack(ENDMAGIC, 0, 0, len(outfiles), len(outfiles), out.tell() - centralstart, centralstart, 0)
	out.write(end)
	out.close()

if __name__ == "__main__":
	from argparse import ArgumentParser
	parser = ArgumentParser()
	parser.add_argument("listfile",
	                    nargs="?",
	                    help="path to list file")
	parser.add_argument("-o", "--output",
	                    default=DEFAULTOUTPUT,
	                    help=f"path to output (default: {DEFAULTOUTPUT})")
	parser.add_argument("-b", "--basepath",
	                    default="",
	                    help="base path to remove from filenames")
	parser.add_argument("-j", "--jobs",
	                    default="1",
	                    help="number of files to deflate concurrently")
	parser.add_argument("-l", "--level",
	                    default="15",
	                    help="Zopfli iterations (default: 15)")
	parser.add_argument("-f", "--force",
	                    action="store_true",
	                    help="force the archive to be rewritten")
	parser.add_argument("-s", "--strip",
	                    action="store_true",
	                    help="strip filenames from local headers "
	                    "(may or may not be allowed by the spec)")
	parser.add_argument("-c", "--comment",
	                    default="",
	                    help="add comment to first file of archive")
	parser.add_argument("-z", "--zlib",
	                    action="store_true",
	                    help="use zlib instead of Zopfli (instant output, worse compression)")
	args = parser.parse_args()

	asyncio.run(main(args))
