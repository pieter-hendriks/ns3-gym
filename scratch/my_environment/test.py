#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from ns3gym import ns3env
from tensorflow.keras.models import Sequential, Model
from tensorflow.keras.layers import Dense, Activation, Flatten, Input, Concatenate
from tensorflow.keras.optimizers import Adam
from rl.agents import DDPGAgent
from rl.agents import DQNAgent
from rl.memory import SequentialMemory
from rl.random import OrnsteinUhlenbeckProcess
from rl.policy import EpsGreedyQPolicy, LinearAnnealedPolicy
from rl.core import Processor
import ctypes
import random
import tensorflow as tf
import itertools

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


ENV_NAME = "MyGymEnv"
class MyProcessor(Processor):
	def __init__(self, n):
		super().__init__()
		self.n = n
	def process_action(self, action):
		action1 = action // self.n
		action2 = action % self.n
		if (action < 0 or action > self.n * self.n or type(action) != int):
			print(action)
			print(self.n*self.n)
			print(type(action))
			assert False
		return [action1 - (self.n//2), action2 - (self.n//2)]
	
	def process_observation(self, obs):
		if not obs:
			obs = [0, 0, 0, 1, 0, 0, 0, 1, 1]
		else:
			fn = lambda x: x if type(x) in [int, float] else x[0]
			obs = [fn(x) for x in obs]
		return obs

parser = argparse.ArgumentParser(description='Start simulation script on/off')
parser.add_argument('--start',
										type=int,
										default=1,
										help='Start ns-3 simulation script 0/1, Default: 1')
parser.add_argument('--iterations',
										type=int,
										default=1,
										help='Number of iterations, Default: 1')
parser.add_argument('--port',
										type=int,
										default=5555,
										help='Port to use for the connection.')
parser.add_argument('--eval',
										type=int,
										default=0,
										help='Set eval to 1 to run evaluation only, with saved weights from current directory.')
parser.add_argument('--no_test', type=int, default=0, help='Set to 1 to disable testing')
parser.add_argument('--save_weights', type=int, default=0, help='Set to 1 to save weights to file.')
parser.add_argument('--load_weights', type=int, default=1, help='Set to 0 to disable weight loading.')


args = parser.parse_args()
startSim = bool(args.start)
iterationNum = int(args.iterations)
runEvalOnly = bool(args.eval)
save_weights = bool(args.save_weights)
load_weights = bool(args.load_weights)
port = int(args.port)
disable_test = bool(args.no_test)
assert not (disable_test and runEvalOnly)
simTime = 600 # seconds
stepTime = 5 # seconds
seed = random.randint(0, 150000)
simArgs = {"--simTime": simTime,
					 "--stepTime": stepTime,
					 "--testArg": 123}
debug = False


try:
	env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)
	# simpler:
	#env = ns3env.Ns3Env()
	env.reset()
	ob_space = env.observation_space
	ac_space = env.action_space
	assert ac_space.spaces[0].n == ac_space.spaces[1].n
	nb_actions = ac_space.spaces[0].n ** 2

	stepIdx = 0
	currIt = 0

	ob_shape_dim = len(ob_space.spaces)
	observation_input = Input(shape=(1,ob_shape_dim), name='observation_input')

	action_input = Input(shape=(nb_actions,), name='action_input')
	actor = Sequential()
	actor = Flatten()(observation_input)
	actor = Dense(16, activation='relu')(actor)
	actor = Dense(32, activation='relu')(actor)
	actor = Dense(16, activation='relu')(actor)
	actor = Dense(32, activation='relu')(actor)
	actor = Dense(nb_actions, activation='linear')(actor)

	model = Model(inputs=observation_input, outputs=actor)
	memory = SequentialMemory(limit=50000, window_length=1)
	policy = LinearAnnealedPolicy(EpsGreedyQPolicy(), attr='eps', value_max=1., value_min=.3, value_test=0, nb_steps=6000)
	agent = DQNAgent(model=model, nb_actions=nb_actions, memory=memory, nb_steps_warmup=100, target_model_update=1e-2, policy=policy, processor=MyProcessor(ac_space.spaces[0].n))
	agent.compile(Adam(lr=1e-2), metrics=['mae'])

	if args.load_weights:
		agent.load_weights('dqn_{}_weights.h5f'.format(ENV_NAME))
	if not runEvalOnly:
		agent.fit(env, nb_steps=10000, visualize=False, verbose=2, nb_max_episode_steps=10)

		# After training is done, we save the final weights.
		if args.save_weights:
			agent.save_weights('dqn_{}_weights.h5f'.format(ENV_NAME), overwrite=True)

		env.reset()
		if not disable_test:
			agent.test(env, nb_episodes=1, visualize=True, nb_max_episode_steps=100)
	else:
		agent.load_weights('dqn_{}_weights.h5f'.format(ENV_NAME))
		agent.test(env, nb_episodes=1, visualize=True, nb_max_episode_steps=100)
except KeyboardInterrupt:
	print("Ctrl-C -> Exit")
finally:
	env.close()
	print("Done")


# OLD AGENT BELOW:
	# actor = Sequential()
	# actor.add(Flatten(input_shape=(1,) + obs_shape))
	# actor.add(Dense(16))
	# actor.add(Activation('relu'))
	# actor.add(Dense(8))
	# actor.add(Activation('relu'))
	# actor.add(Dense(4))
	# actor.add(Activation('relu'))
	# actor.add(Dense(nb_actions)) # Only one output
	# actor.add(Activation('linear'))
	# # print(actor.summary())
	# # print(env.observation_space)
	# # print(env.observation_space.shape)
	# # input("Pause point")
	# observation_input = Input(shape=(1,) + obs_shape, name='observation_input')
	# action_input = Input(shape=(nb_actions,), name='action_input')
	# flattened_observation = Flatten()(observation_input)
	# x = Concatenate()([action_input, flattened_observation])
	# x = Dense(16)(x)
	# x = Activation('relu')(x)
	# x = Dense(8)(x)
	# x = Activation('relu')(x)
	# x = Dense(4)(x)
	# x = Activation('relu')(x)
	# x = Dense(1)(x)
	# x = Activation('linear')(x)
	# critic = Model(inputs=[action_input, observation_input], outputs=x)
	# # print(critic.summary())
	
	# 
	# random_process = OrnsteinUhlenbeckProcess(size=nb_actions, theta=1, mu=0., sigma=3)
	# agent = DDPGAgent(nb_actions=nb_actions, actor=actor, critic=critic, critic_action_input=action_input,
	# 									memory=memory, nb_steps_warmup_critic=400, nb_steps_warmup_actor=400,
	# 									random_process=random_process, gamma=.995, target_model_update=2e-3, processor = MyProcessor())
