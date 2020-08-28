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
from rl.policy import EpsGreedyQPolicy, LinearAnnealedPolicy, GreedyQPolicy
from rl.core import Processor
from keras import backend as K
import ctypes
import random
import tensorflow as tf
import itertools

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


ENV_NAME = "MyGymEnv"

import math

class MyProcessor(Processor):
	def __init__(self, n):
		super().__init__()
		self.n = n
		self.actionSize = self.n // 2
		self.lastObs = None
		self.decrease = False
		
		
	def process_action(self, action):
		return [action - (self.n//2)]
		#action1 = action // self.n
		#action2 = action % self.n
		#if (action < 0 or action > self.n * self.n):
		#	print(action)
		#	print(self.n*self.n)
		#	print(type(action))
		#	assert False
		#return [action1 - (self.n//2), action2 - (self.n//2)]
		
		# Non-RL naive/simple algorithm implementation
		#action = 0
		#if self.lastObs is not None:
		#	if self.lastObs[0] == 1:
		#		if self.decrease and self.actionSize > 1:
		#			self.actionSize = int(self.actionSize / 2)
		#			self.decrease = False
		#		action = self.actionSize
		#	elif self.lastObs[1] == 1:
		#		self.decrease = True
		#		action = -1 * self.actionSize
		#	elif self.lastObs[2] == 1:
		#		self.decrease = True
		#		action = -1 * self.actionSize
		#	elif self.lastObs[3] == 1:
		#		self.decrease = True
		#		action = -3 * self.actionSize
		#	else:
		#		self.decrease = True
		#		action = self.actionSize
		#action = int(action)
		#action = max(min(action, 32), -32)
		#print (f"{action=}")
		#print (f"{self.lastObs=}")
		#return [action, -32]
	
	def process_observation(self, obs):
		if not obs:
			# obs = [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1]
			obs = [0, 0, 0, 0, 0, 1]
		else:
			# Unpack/flatten the box spaces to singular values
			fn = lambda x: x if type(x) in [int, float] else x[0]
			obs = [fn(x) for x in obs]
		obs.append(1)
		self.lastObs = obs
		return obs
		


parser = argparse.ArgumentParser(description='Start simulation script on/off')
parser.add_argument('--start', type=int, default=1, help='Start ns-3 simulation script 0/1, Default: 1')
parser.add_argument('--iterations', type=int, default=1, help='Number of iterations, Default: 1')
parser.add_argument('--port', type=int, default=5555, help='Port to use for the connection.')
parser.add_argument('--eval', type=int, default=0, help='Set eval to 1 to run evaluation only, with saved weights from current directory.')
parser.add_argument('--no_test', type=int, default=0, help='Set to 1 to disable testing')
parser.add_argument('--save_weights', type=int, default=0, help='Set to 1 to save weights to file.')
parser.add_argument('--load_weights', type=int, default=1, help='Set to 0 to disable weight loading.')


beta = 1
alpha = 1
def swish(x):
	return K.sigmoid(x * beta) * alpha * x

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
simArgs = {"--simTime": simTime, "--stepTime": stepTime, "--testArg": 123}
debug = False

try:
	env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)
	# simpler:
	#env = ns3env.Ns3Env()
	env.reset()
	ob_space = env.observation_space
	ac_space = env.action_space
	#assert ac_space.spaces[0].n == ac_space.spaces[1].n
	nb_actions = ac_space.spaces[0].n

	stepIdx = 0
	currIt = 0

	ob_shape_dim = len(ob_space.spaces) + 1
	observation_input = Input(shape=(1,ob_shape_dim), name='observation_input')
	action_input = Input(shape=(nb_actions,), name='action_input')
	actor = Flatten()(observation_input)
	actor = Dense(48)(actor)
	actor = tf.keras.layers.LeakyReLU()(actor)
	actor = Dense(36)(actor)
	actor = tf.keras.layers.LeakyReLU()(actor)
	actor = Dense(30)(actor)
	actor = tf.keras.layers.LeakyReLU()(actor)
	#actor = Dense(32)(actor)
	#actor = tf.keras.layers.LeakyReLU()(actor)
	actor = Dense(nb_actions, activation='softmax')(actor)

	model = Model(inputs=observation_input, outputs=actor)
	memory = SequentialMemory(limit=3000, window_length=1)
	policy = LinearAnnealedPolicy(EpsGreedyQPolicy(), attr='eps', value_max=1.0, value_min=0.15, value_test=0.0, nb_steps=5000)
	agent = DQNAgent(model=model, nb_actions=nb_actions, memory=memory, nb_steps_warmup=150, target_model_update=25, policy=policy, processor=MyProcessor(ac_space.spaces[0].n), test_policy=GreedyQPolicy())
	agent.compile(Adam(lr=1e-2), metrics=['mae'])
	
	
	if not runEvalOnly:
		if args.load_weights:
			agent.load_weights('dqn_{}_weights.h5f'.format(ENV_NAME))
		agent.fit(env, nb_steps=5000, visualize=False, verbose=2, nb_max_episode_steps=125)

		# After training is done, we save the final weights.
		if args.save_weights:
			agent.save_weights('dqn_{}_weights.h5f'.format(ENV_NAME), overwrite=True)

		if not disable_test:
			env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)
			agent.test(env, nb_episodes=1, visualize=True, nb_max_episode_steps=100)
	else:
		#agent.load_weights('dqn_{}_weights.h5f'.format(ENV_NAME))
		agent.test(env, nb_episodes=10, visualize=True, nb_max_episode_steps=100)
except KeyboardInterrupt:
	print("Ctrl-C -> Exit")
finally:
	env.close()
	print("Done")
