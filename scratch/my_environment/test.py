#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from ns3gym import ns3env
from keras.models import Sequential, Model
from keras.layers import Dense, Activation, Flatten, Input, Concatenate
from keras import activations
from keras.optimizers import Adam
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
		if (action < 0 or action > self.n * self.n):
			print(action)
			print(self.n*self.n)
			print(type(action))
			assert False
		return [action1 - (self.n//2), action2 - (self.n//2)]
	
	def process_observation(self, obs):
		if not obs:
			obs = [-1., 0, 1, -1., 0, 1, 0.5, 1]
		else:
			# Unpack/flatten the box spaces to singular values
			fn = lambda x: x if type(x) in [int, float] else x[0]
			obs = [fn(x) for x in obs]
			obs.append(1)
		# print (obs)
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
parser.add_argument('--save_weights', type=int, default=1, help='Set to 1 to save weights to file.')
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
simTime = 1250 # seconds
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

	ob_shape_dim = len(ob_space.spaces) + 1
	observation_input = Input(shape=(1,ob_shape_dim), name='observation_input')

	action_input = Input(shape=(nb_actions,), name='action_input')
	actor = Flatten()(observation_input)
	actor = Dense(12, activation='linear')(actor)
	actor = Dense(8, activation='elu')(actor)
	actor = Dense(4, activation='tanh')(actor)
	actor = Dense(nb_actions, activation='linear')(actor)

	model = Model(inputs=observation_input, outputs=actor)
	memory = SequentialMemory(limit=50000, window_length=1)
	policy = LinearAnnealedPolicy(EpsGreedyQPolicy(), attr='eps', value_max=1., value_min=.2, value_test=0.05, nb_steps=10000)
	agent = DQNAgent(model=model, nb_actions=nb_actions, memory=memory, nb_steps_warmup=100, target_model_update=1e-3, policy=policy, processor=MyProcessor(ac_space.spaces[0].n))
	agent.compile(Adam(lr=1e-3), metrics=['mae'])

	if args.load_weights:
		agent.load_weights('dqn_{}_weights.h5f'.format(ENV_NAME))
	if not runEvalOnly:
		agent.fit(env, nb_steps=10000, visualize=False, verbose=1, nb_max_episode_steps=250)

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
