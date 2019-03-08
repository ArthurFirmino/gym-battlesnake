# Gym-Battlesnake

Gym-Battlesnake is a multi-agent reinforcement learning environment inspired by the annual Battlesnake event held in Victoria, BC each year, and conforming to the OpenAI Gym interface.

![Alt Text](https://github.com/ArthurFirmino/gym-battlesnake/raw/master/render.gif)

## Features

  - Multi-threaded game implementation written in fast C++
  - Single agent training with multiple other agents as opponents
  - Render mode available to see your agents play

## Installation
### Prerequisites
Gym-Battlesnake has only been tested on **Ubuntu 18.04**. Install the dependencies using the command:
```
sudo apt-get update && sudo apt-get install cmake libopenmpi-dev python3-dev zlib1g-dev libsfml-dev
```
You will also need to install `tensorflow` or `tensorflow-gpu`  (2.0 **not** tested), see https://www.tensorflow.org/install/pip.

### Install using Pip
Clone this repository using the following command:
```sh
git clone https://github.com/ArthurFirmino/gym-battlesnake
```
Change into the directory and install using pip (consider setting up a Python virtual environment first):
```sh
cd gym-battlesnake
pip install -e .
```

## Example
**Single agent training:**
```python
from gym_battlesnake.gymbattlesnake import BattlesnakeEnv
from gym_battlesnake.custompolicy import CustomPolicy
from stable_baselines import PPO2

env = BattlesnakeEnv(n_threads=4, n_envs=16)

model = PPO2(CustomPolicy, env, verbose=1, learning_rate=1e-3)
model.learn(total_timesteps=100000)
model.save('ppo2_trainedmodel')

del model
model = PPO2.load('ppo2_trainedmodel')

obs = env.reset()
for _ in range(10000):
    action,_ = model.predict(obs)
    obs,_,_,_ = env.step(action)
    env.render()
```
**Multi agent training:**
```python
from gym_battlesnake.gymbattlesnake import BattlesnakeEnv
from gym_battlesnake.custompolicy import CustomPolicy
from stable_baselines import PPO2

num_agents = 4
placeholder_env = BattlesnakeEnv(n_threads=4, n_envs=16)
models = [PPO2(CustomPolicy, placeholder_env, verbose=1, learning_rate=1e-3) for _ in range(num_agents)]
placeholder_env.close()

for _ in range(10):
    for model in models:
        env = BattlesnakeEnv(n_threads=4, n_envs=16, opponents=[ m for m in models if m is not model])
        model.set_env(env)
        model.learn(total_timesteps=100000)
        env.close()

model = models[0]
env = BattlesnakeEnv(n_threads=1, n_envs=1, opponents=[ m for m in models if m is not model])
obs = env.reset()
for _ in range(10000):
    action,_ = model.predict(obs)
    obs,_,_,_ = env.step(action)
    env.render()
```
## Notes
  - See OpenAI documentation on `gym` for more details about its interface
  - See `stable-baselines` documentation for more details on their PPO2 implementation and other suitable algorithms
  - For multi-agent training `tensorflow-gpu` is recommended, as well as a large number of environments (~100) to maximize data transfer to the GPU.

## Contributing
 1. **Fork**
 2.  **Clone and Setup**
 3. **Develop**
 4.  **Pull Request**
