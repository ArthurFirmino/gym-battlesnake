import numpy as np
import ctypes
import pathlib
from time import sleep
from gym import spaces
from stable_baselines.common.vec_env import VecEnv

def wrap_function(lib, funcname, restype, argtypes):
    """Simplify wrapping ctypes functions"""
    func = lib.__getattr__(funcname)
    func.restype = restype
    func.argtypes = argtypes
    return func

class info(ctypes.Structure):
    _fields_ = [('health', ctypes.c_uint), ('length', ctypes.c_uint), ('turn', ctypes.c_uint),
        ('alive', ctypes.c_bool), ('ate', ctypes.c_bool), ('over', ctypes.c_bool)]

gamelib = ctypes.cdll.LoadLibrary(str(pathlib.Path(__file__).with_name('libgymbattlesnake.so')))
env_new = wrap_function(gamelib, 'env_new', ctypes.c_void_p, [ctypes.c_uint,ctypes.c_uint,ctypes.c_uint])
env_delete = wrap_function(gamelib, 'env_delete', None, [ctypes.c_void_p])
env_reset = wrap_function(gamelib, 'env_reset', None, [ctypes.c_void_p])
env_step = wrap_function(gamelib, 'env_step', None, [ctypes.c_void_p])
env_render = wrap_function(gamelib, 'env_render', None, [ctypes.c_void_p])
env_obsptr = wrap_function(gamelib, 'env_getobspointer', ctypes.POINTER(ctypes.c_ubyte), [ctypes.c_void_p,ctypes.c_uint])
env_actptr = wrap_function(gamelib, 'env_getactpointer', ctypes.POINTER(ctypes.c_ubyte), [ctypes.c_void_p,ctypes.c_uint])
env_infoptr = wrap_function(gamelib, 'env_getinfopointer', ctypes.POINTER(info), [ctypes.c_void_p])

NUM_LAYERS = 6
LAYER_WIDTH = 39
LAYER_HEIGHT = 39

class BattlesnakeEnv(VecEnv):
    """Multi-Threaded Multi-Agent Snake Environment"""
    metadata = {'render.modes': ['human']}

    def __init__(self, n_threads=4, n_envs=16, opponents=[]):
        # Define action and observation space
        self.action_space = spaces.Discrete(4)
        self.observation_space = spaces.Box(low=0,high=255, shape=(LAYER_WIDTH, LAYER_HEIGHT, NUM_LAYERS), dtype=np.uint8)
        self.n_opponents = len(opponents)
        self.opponents = opponents
        self.n_threads = n_threads
        self.n_envs = n_envs
        self.ptr = env_new(self.n_threads, self.n_envs, self.n_opponents+1)
        super(BattlesnakeEnv, self).__init__(self.n_envs, self.observation_space, self.action_space)
        self.reset()

    def close(self):
        env_delete(self.ptr)

    def step_async(self, actions):
        # Write player actions into buffer
        np.copyto(self.getact(0), np.asarray(actions,dtype=np.uint8))
        # Get observations for each opponent and predict actions
        for i in range(1,self.n_opponents+1):
            obss = self.getobs(i)
            acts,_ = self.opponents[i-1].predict(obss, deterministic=True)
            np.copyto(self.getact(i), np.asarray(acts,dtype=np.uint8))
        # Step game
        env_step(self.ptr)

    def step_wait(self):

        info = [{} for _ in range(self.n_envs)]
        dones = np.asarray([ False for _ in range(self.n_envs) ])
        rews = np.zeros((self.n_envs))

        infoptr = env_infoptr(self.ptr)
        for i in range(self.n_envs):
            if infoptr[i].ate:
                rews[i] += 0.1
            if infoptr[i].over:
                dones[i] = True
                info[i]['episode'] = {}
                if infoptr[i].alive:
                    rews[i] += 1.0 if infoptr[i].turn > 100 else 0.0
                    info[i]['episode']['r'] = 1.0 if infoptr[i].turn > 100 else 0.0
                else:
                    rews[i] -= 1.0
                    info[i]['episode']['r'] = -1.0
                info[i]['episode']['l'] = infoptr[i].turn

        return self.getobs(0), rews, dones, info

    def reset(self):
        env_reset(self.ptr)
        return self.getobs(0)

    def render(self):
        env_render(self.ptr)
        sleep(0.1)

    def getobs(self, agent_i):
        obsptr = env_obsptr(self.ptr, agent_i)
        return np.ctypeslib.as_array(obsptr, shape=(self.n_envs, LAYER_WIDTH, LAYER_HEIGHT, NUM_LAYERS))

    def getact(self, agent_i):
        actptr = env_actptr(self.ptr, agent_i)
        return np.ctypeslib.as_array(actptr, shape=(self.n_envs,))