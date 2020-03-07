#pragma once
#ifndef INC_MYENV_ENVIRONMENT_H_
#define INC_MYENV_ENVIRONMENT_H_

#include <ns3/simulator.h>
class MyGymEnv;
auto defineMyEnvironment(MyGymEnv& gym) -> void;

#endif