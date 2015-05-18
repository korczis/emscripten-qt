#!/usr/bin/env python 

import os, sys, shutil, tempfile, subprocess, shlex, time 
from subprocess import PIPE, STDOUT 
from tools import js_optimizer 
from tools import shared 
#from tools.shared import Compression, execute, suffix, unsuffixed, unsuffixed_basename 

filename = sys.argv[1]

with file(filename) as f:
    final = f.read()

final = js_optimizer.run(filename, ['reduceUnusedVarDecs', 'reduceVariableScopes', 'globalMinify', 'compress', 'mangle'], shared.NODE_JS, False)
#final = js_optimizer.run(filename, ['globalMinify' ], shared.NODE_JS, False)
#final = js_optimizer.run(filename, [], shared.NODE_JS, False)
#final = js_optimizer.run(filename, ['registerize', 'reduceVariableScopes'], shared.NODE_JS, False)
#final = js_optimizer.run(filename, ['compress', 'mangle'], shared.NODE_JS, False)

#final = shared.Building.js_optimizer(final, ['registerize'], false)

print final

