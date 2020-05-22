#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from ns3gym import ns3env
from tensorflow.keras.models import Sequential, Model
from tensorflow.keras.layers import Dense, Activation, Flatten, Input, Concatenate
from tensorflow.keras.optimizers import Adam
from rl.agents import DDPGAgent
from rl.memory import SequentialMemory
from rl.random import OrnsteinUhlenbeckProcess
from rl.core import Processor
import ctypes

import tensorflow as tf

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


ENV_NAME = "MyGymEnv"

class MyProcessor(Processor):
	def __init__(self):
		super().__init__()

	def process_action(self, action):
		# Agent outputs a 32-bit unsigned number as action,
		# z3mq bridge allows up to 32-bit signed max values, so have to convert unsigned to signed.

		action = ctypes.c_int32(action).value
		if action < 0:
			action = 0
		return action



parser = argparse.ArgumentParser(description='Start simulation script on/off')
parser.add_argument('--start',
										type=int,
										default=1,
										help='Start ns-3 simulation script 0/1, Default: 1')
parser.add_argument('--iterations',
										type=int,
										default=1,
										help='Number of iterations, Default: 1')
args = parser.parse_args()
startSim = bool(args.start)
iterationNum = int(args.iterations)

port = 5555
simTime = 300 # seconds
stepTime = 0.1  # seconds
seed = 0
simArgs = {"--simTime": simTime,
					 "--stepTime": stepTime,
					 "--testArg": 123}
debug = False

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)

try:
		# simpler:
		#env = ns3env.Ns3Env()
		env.reset()

		ob_space = env.observation_space
		ac_space = env.action_space
		nb_actions = 1 # env.action_space.shape[0]

		stepIdx = 0
		currIt = 0
		actor = Sequential()
		actor.add(Flatten(input_shape=(1,) + env.observation_space.shape))
		actor.add(Dense(32))
		actor.add(Activation('relu'))
		actor.add(Dense(32))
		actor.add(Activation('relu'))
		actor.add(Dense(32))
		actor.add(Activation('relu'))
		actor.add(Dense(nb_actions)) # Only one output
		actor.add(Activation('linear'))
		# print(actor.summary())
		# print(env.observation_space)
		# print(env.observation_space.shape)
		# input("Pause point")
		action_input = Input(shape=(nb_actions,), name='action_input')
		observation_input = Input(shape=(1,) + env.observation_space.shape, name='observation_input')
		flattened_observation = Flatten()(observation_input)
		x = Concatenate()([action_input, flattened_observation])
		x = Dense(32)(x)
		x = Activation('relu')(x)
		x = Dense(64)(x)
		x = Activation('relu')(x)
		x = Dense(32)(x)
		x = Activation('relu')(x)
		x = Dense(1)(x)
		x = Activation('linear')(x)
		critic = Model(inputs=[action_input, observation_input], outputs=x)
		# print(critic.summary())

		memory = SequentialMemory(limit=100000, window_length=1)
		random_process = OrnsteinUhlenbeckProcess(size=nb_actions, theta=.15, mu=0., sigma=.3)
		agent = DDPGAgent(nb_actions=nb_actions, actor=actor, critic=critic, critic_action_input=action_input,
											memory=memory, nb_steps_warmup_critic=100, nb_steps_warmup_actor=100,
											random_process=None, gamma=.99, target_model_update=1e-3, processor = MyProcessor())
		optimizer = Adam(lr=.001, clipnorm=1.)
		optimizer._name = 'Adam'
		agent.compile(optimizer, metrics=['mae'])
		print("Fitting")
		agent.fit(env, nb_steps=10000, visualize=False, verbose=1, nb_max_episode_steps=2500)
		print("End fitting")
		# After training is done, we save the final weights.
		# agent.save_weights('ddpg_{}_weights.h5f'.format(ENV_NAME), overwrite=True)

		# Finally, evaluate our algorithm for 5 episodes.
		agent.test(env, nb_episodes=5, visualize=True, nb_max_episode_steps=200)

except KeyboardInterrupt:
		print("Ctrl-C -> Exit")
finally:
		env.close()
		print("Done")
