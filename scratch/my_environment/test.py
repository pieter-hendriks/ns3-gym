#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from ns3gym import ns3env
from keras.models import Sequential, Model
from keras.layers import Dense, Activation, Flatten, Input, Concatenate
from keras.optimizers import Adam
from rl.agents import DDPGAgent
from rl.memory import SequentialMemory
from rl.random import OrnsteinUhlenbeckProcess

import tensorflow as tf

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


ENV_NAME = "MyGymEnv"

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
simTime = 5 # seconds
stepTime = 0.5  # seconds
seed = 0
simArgs = {"--simTime": simTime,
           "--stepTime": stepTime,
           "--testArg": 123}
debug = False

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)

def rescaleFunction(x):
    fn = lambda y: tf.mod(tf.abs(y), tf.constant(2, dtype=tf.float32))
    return tf.map_fn(fn, x, dtype=tf.uint32)

try:
    # simpler:
    #env = ns3env.Ns3Env()
    env.reset()

    ob_space = env.observation_space
    ac_space = env.action_space
    assert(len(env.action_space.shape) == 1)
    nb_actions = env.action_space.shape[0]

    stepIdx = 0
    currIt = 0

    actor = Sequential()
    actor.add(Flatten(input_shape=(1,) + env.observation_space.shape))
    actor.add(Dense(16))
    actor.add(Activation('relu'))
    actor.add(Dense(16))
    actor.add(Activation('relu'))
    actor.add(Dense(16))
    actor.add(Activation('relu'))
    actor.add(Dense(nb_actions))
    actor.add(Activation('linear'))
    print(actor.summary())

    action_input = Input(shape=(nb_actions,), name='action_input')
    observation_input = Input(shape=(1,) + env.observation_space.shape, name='observation_input')
    flattened_observation = Flatten()(observation_input)
    x = Concatenate()([action_input, flattened_observation])
    x = Dense(32)(x)
    x = Activation('relu')(x)
    x = Dense(32)(x)
    x = Activation('relu')(x)
    x = Dense(32)(x)
    x = Activation('relu')(x)
    x = Dense(1)(x)
    x = Activation('linear')(x)
    critic = Model(inputs=[action_input, observation_input], outputs=x)
    print(critic.summary())

    memory = SequentialMemory(limit=100000, window_length=1)
    random_process = OrnsteinUhlenbeckProcess(size=ac_space.shape, theta=.15, mu=0., sigma=.3)
    agent = DDPGAgent(nb_actions=nb_actions, actor=actor, critic=critic, critic_action_input=action_input,
                      memory=memory, nb_steps_warmup_critic=100, nb_steps_warmup_actor=100,
                      random_process=random_process, gamma=.99, target_model_update=1e-3)
    agent.compile(Adam(lr=.001, clipnorm=1.), metrics=['mae'])

    agent.fit(env, nb_steps=10000, visualize=False, verbose=1, nb_max_episode_steps=2500)

    # After training is done, we save the final weights.
    agent.save_weights('ddpg_{}_weights.h5f'.format(ENV_NAME), overwrite=True)

    # Finally, evaluate our algorithm for 5 episodes.
    agent.test(env, nb_episodes=1, visualize=True, nb_max_episode_steps=200)


    # while True:
    #     print("Start iteration: ", currIt)
    #     obs = env.reset()
    #     print("Step: ", stepIdx)
    #     print("---obs: ", obs)

    #     while True:
    #         action = env.action_space.sample()
    #         print(action)
    #         print("---action: ", action)

    #         print("Step: ", stepIdx)
    #         stepIdx += 1
    #         obs, reward, done, info = env.step(action)
    #         print("---obs, reward, done, info: ", obs, reward, done, info)

    #         if done:
    #             stepIdx = 0
    #             if currIt + 1 < iterationNum:
    #                 env.reset()
    #             print("Finished")
    #             break

    #     currIt += 1
    #     if currIt == iterationNum:
    #         break

except KeyboardInterrupt:
    print("Ctrl-C -> Exit")
finally:
    env.close()
    print("Done")
