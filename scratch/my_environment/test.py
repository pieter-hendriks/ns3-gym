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

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


ENV_NAME = "MyGymEnv"

class MyProcessor(Processor):
	def __init__(self):
		super().__init__()

	def process_action(self, action):
		print(f"Unprocessed action = {action}")
		# Action is change in allowed flowCount. 
		action = abs(int(action)) % 65 # This should be plenty for the stream type we're doing. Each stream is currently 192 kbps.
		action -= 32

		# action = ctypes.c_int32(action).value
		print(f"Acting: {action}")
		return action
	
	def process_observation(self, obs):
		if obs:
			obs.append(25)
			return obs
		return [0, 0, 0, 0, 10, 25]

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
parser.add_argument('--save_weights', type=int, default=0, help='Set to 1 to save weights to file.')

args = parser.parse_args()
startSim = bool(args.start)
iterationNum = int(args.iterations)
runEvalOnly = bool(args.eval)
save_weights = bool(args.save_weights)
port = int(args.port)
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

	nb_actions = ac_space.n # env.action_space.shape[0]

	stepIdx = 0
	currIt = 0
	# We get a single dimensional observation -
	# But we add a single item to it (constant term) to avoid stuck-at-zero problem.
	obs_shape = list(env.observation_space.shape)
	obs_shape[0] += 1
	obs_shape = tuple(obs_shape)
	assert(len(obs_shape) == 1) # If this isn't true anymore, logic needs to be changed.
	observation_input = Input(shape=(1,) + obs_shape, name='observation_input')
	action_input = Input(shape=(nb_actions,), name='action_input')
	actor = Sequential()
	actor = Flatten()(observation_input)
	actor = Dense(64, activation='relu')(actor)
	actor = Dense(48, activation='relu')(actor)
	actor = Dense(32, activation='relu')(actor)
	actor = Dense(16, activation='relu')(actor)
	actor = Dense(nb_actions, activation='linear')(actor)

	model = Model(inputs=observation_input, outputs=actor)
	memory = SequentialMemory(limit=50000, window_length=1)
	policy = LinearAnnealedPolicy(EpsGreedyQPolicy(), attr='eps', value_max=1., value_min=.1, value_test=0.001, nb_steps=8000)
	agent = DQNAgent(model=model, nb_actions=nb_actions, memory=memory, nb_steps_warmup=50, target_model_update=1e-2, policy=policy, processor=MyProcessor())
	agent.compile(Adam(lr=5e-3), metrics=['mae'])


	# agent = DQNAgent(model, memory, enable_double_dqn=True, enable_dueling_network=True, dueling_type='avg', nb_actions=nb_actions, model=model, processor=MyProcessor, nb_steps_warmup=250)

	#optimizer = Adam(lr=.002, clipnorm=1.)
	#optimizer._name = 'Adam'
	#agent.compile(optimizer, metrics=['mae'])
	if not runEvalOnly:
		agent.fit(env, nb_steps=5000, visualize=False, verbose=2, nb_max_episode_steps=500)
		#dqn.fit(env, nb_steps = 5000, visualize=False, verbose=2, nb_max_episode_steps=500)

		# After training is done, we save the final weights.
		if args.save_weights:
			agent.save_weights('ddpg_{}_weights.h5f'.format(ENV_NAME), overwrite=True)

		env.reset()
		agent.test(env, nb_episodes=1, visualize=True, nb_max_episode_steps=100)
	else:
		agent.load_weights('ddpg_{}_weights.h5f'.format(ENV_NAME))
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