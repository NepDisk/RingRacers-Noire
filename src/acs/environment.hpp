// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2016 by James Haley, David Hill, et al. (Team Eternity)
// Copyright (C) 2022 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2022 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  environment.hpp
/// \brief Action Code Script: Environment definition

#ifndef __SRB2_ACS_ENVIRONMENT_HPP__
#define __SRB2_ACS_ENVIRONMENT_HPP__

#include <ACSVM/Code.hpp>
#include <ACSVM/CodeData.hpp>
#include <ACSVM/Environment.hpp>
#include <ACSVM/Error.hpp>
#include <ACSVM/Module.hpp>
#include <ACSVM/Scope.hpp>
#include <ACSVM/Script.hpp>
#include <ACSVM/Serial.hpp>
#include <ACSVM/Thread.hpp>
#include <Util/Floats.hpp>

namespace srb2::acs {

class Environment : public ACSVM::Environment
{
public:
	Environment();

	virtual bool checkTag(ACSVM::Word type, ACSVM::Word tag);

	virtual ACSVM::Word callSpecImpl(
		ACSVM::Thread *thread, ACSVM::Word spec,
		const ACSVM::Word *argV, ACSVM::Word argC
	);

	virtual ACSVM::Thread *allocThread();

protected:
	virtual void loadModule(ACSVM::Module *module);
};

}

extern srb2::acs::Environment ACSEnv;

#endif // __SRB2_ACS_ENVIRONMENT_HPP__
