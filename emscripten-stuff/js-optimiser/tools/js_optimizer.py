
import os, sys, subprocess, multiprocessing, re
import shared

obfuscate_globals = True;

temp_files = shared.TempFiles()

__rootpath__ = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
def path_from_root(*pathelems):
    return os.path.join(__rootpath__, *pathelems)

JS_OPTIMIZER = path_from_root('tools', 'js-optimizer.js')
JS_GLOBAL_LISTER = path_from_root('tools', 'js-global-lister.js')

BEST_JS_PROCESS_SIZE = 1024*1024

WINDOWS = sys.platform.startswith('win')

DEBUG = os.environ.get('EMCC_DEBUG')

def run_on_chunk(command):
    filename = command[2] # XXX hackish
    output = subprocess.Popen(command, stdout=subprocess.PIPE).communicate()[0]
    assert len(output) > 0 and not output.startswith('Assertion failed'), 'Error in js optimizer: ' + output
    filename = temp_files.get(os.path.basename(filename) + '.jo.js').name
    f = open(filename, 'w')
    f.write(output)
    f.close()
    return filename

def run(filename, passes, js_engine, jcache):
    if jcache: shared.JCache.ensure()

    if type(passes) == str:
        passes = [passes]

    js = open(filename).read()
    if os.linesep != '\n':
        js = js.replace(os.linesep, '\n') # we assume \n in the splitting code

    #print "All: " + js + "\n---- endall ----\n"

    # Find suffix
    suffix_marker = '// EMSCRIPTEN_GENERATED_FUNCTIONS'
    suffix_start = js.find(suffix_marker)
    suffix = ''
    if suffix_start >= 0:
        suffix = js[suffix_start:js.find('\n', suffix_start)] + '\n'
        # if there is metadata, we will run only on the generated functions. If there isn't, we will run on everything.
        generated = set(eval(suffix[len(suffix_marker)+1:]))

    if not suffix and jcache:
        # JCache cannot be used without metadata, since it might reorder stuff, and that's dangerous since only generated can be reordered
        # This means jcache does not work after closure compiler runs, for example. But you won't get much benefit from jcache with closure
        # anyhow (since closure is likely the longest part of the build).
        if DEBUG: print >>sys.stderr, 'js optimizer: no metadata, so disabling jcache'
        jcache = False

    # If we process only generated code, find that and save the rest on the side
    func_sig = re.compile('function (_[\w$]+)\(')
    if suffix:
        pos = 0
        gen_start = 0
        gen_end = 0
        while 1:
            m = func_sig.search(js, pos)
            if not m: break
            pos = m.end()
            ident = m.group(1)
            if ident in generated:
                if not gen_start:
                    gen_start = m.start()
                    assert gen_start
                gen_end = js.find('\n}\n', m.end()) + 3
        print ("old gen_start: " , gen_start)
        gen_start = js.rfind("var FUNCTION_TABLE", 0, gen_start)
        print ("new gen_start: " , gen_start)
        assert gen_end > gen_start

        pre = js[:gen_start]
        post = js[gen_end:]
        js = js[gen_start:gen_end]
    else:
        pre = ''
        post = ''

    #print("pre(" + str(len(pre)) + "):" + pre);
    #print("\n---------endpre--------");
    #print("post(" + str(len(post)) + "):" + post);
    #print("\n---------endpost--------");
    #print("js(" + str(len(js)) + "):" + js);
    #print("\n---------endjs--------");
    #print("real_pre(" + str(len(real_pre)) + "):" + real_pre);
    #print("\n---------endreal_pre--------");
    if obfuscate_globals:
        global_allocs_start = pre.find("// === Body ===")
        if global_allocs_start == None:
            raise AssertionError("Could not find start of global allocations!")
        heap_alloc_start = pre.find("\nHEAP", global_allocs_start)
        if heap_alloc_start == None:
            raise AssertionError("Could not find start of heap allocations!")
        not_heap_alloc = re.compile('\n[^H]')
        heap_alloc_end = not_heap_alloc.search(pre, heap_alloc_start)
        if heap_alloc_end == None:
            raise AssertionError("Could not find end of heap allocations!")
        global_allocs_end = heap_alloc_end.start() - 1
        global_allocs = pre[global_allocs_start:global_allocs_end]

         #print "global allocs:\n" + pre[global_allocs_start:global_allocs_end] + "\n------"
        real_pre = pre[:global_allocs_start] + pre[global_allocs_end:]
        non_generated = real_pre + post
        #print("non-generated: " + non_generated + "\n-----\n")
        non_generated_filename = temp_files.get('jsnongen.js').name
        f = open(non_generated_filename, 'w')
        f.write(non_generated)
        f.close()
        all_globals_filename = run_on_chunk([js_engine, JS_GLOBAL_LISTER, non_generated_filename, ''])
        all_globals = open(all_globals_filename).read().split("\n")
        #print "all_globals: "  , all_globals , "\n-------\n"
        reserved_names = set(all_globals + ["Module", "var", "do",   "break",
              "case", "catch", "const", "continue", "default", "delete", "do", "else", "finally", "for", "function", "if", "in", "instanceof", "new", "return", "switch", "throw", "try", "typeof", "var", "void", "while", "with", "abstract", "boolean", "byte", "char", "class", "debugger", "double", "enum", "export", "extends", "final", "float", "goto", "implements", "import", "int", "interface", "long", "native", "package", "private", "protected", "public", "short", "static", "super", "synchronized", "throws", "transient", "volatile"])
        no_obfuscate = ["_malloc", "_free", "_main"] + all_globals + filter(lambda x : x.find("EMSCRIPTENQT") != -1,  list(generated))
        obfuscatable_names = list(generated)
        for not_obfuscatable in no_obfuscate:
            if not_obfuscatable in obfuscatable_names:
                obfuscatable_names.remove(not_obfuscatable)
        obfuscatable_names = ["HEAP32"] + obfuscatable_names
        global_dec_regex = re.compile("\nvar ([^;\s]+);")
        next_global_var_dec_search_pos = 0
        while True:
            global_var_dec = global_dec_regex.search(global_allocs, next_global_var_dec_search_pos)
            if global_var_dec == None:
                break
            obfuscatable_names.append(global_var_dec.group(1))

            next_global_var_dec_search_pos = global_var_dec.end()

        print ("obfuscatable names: " , obfuscatable_names)
        obfuscated_name = {}
        obfuscated_name_chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_'
        current_obfuscated_name = ['a', 'a']
        for obfuscatable_name in obfuscatable_names:
            while True:
                current_char_index = len(current_obfuscated_name) - 1
                while True:
                    blah = obfuscated_name_chars.find(current_obfuscated_name[current_char_index])
                    if blah + 1 == len(obfuscated_name_chars):
                        current_obfuscated_name[current_char_index] = obfuscated_name_chars[0]
                        if current_char_index == 0:
                            current_obfuscated_name = [obfuscated_name_chars[0]] + current_obfuscated_name
                        else:
                            current_char_index = current_char_index - 1
                    else:
                        current_obfuscated_name[current_char_index] = obfuscated_name_chars[blah + 1]
                        break
                if "".join(current_obfuscated_name) not in reserved_names:
                    break
            obfuscated_name[obfuscatable_name] = "".join(current_obfuscated_name)
            print "obfuscating " + obfuscatable_name + " as " + "".join(current_obfuscated_name)


        obfuscated_names_by_size = sorted(list(obfuscatable_names), key=lambda name: len(name))
        obfuscated_names_by_size.reverse()

        suffix = suffix + "\n// EMSCRIPTEN_FUNCTION_OBFUSCATION: ["
        for obfuscatable_name in obfuscated_names_by_size:
            print "goobles: " + obfuscatable_name
        #for obfuscatable_name in obfuscated_names_by_size:
              #global_allocs = global_allocs.replace(obfuscatable_name, obfuscated_name[obfuscatable_name])
              #post = post.replace(obfuscatable_name, obfuscated_name[obfuscatable_name])
            #suffix = suffix.replace(obfuscatable_name, obfuscated_name[obfuscatable_name])
            suffix = suffix + "\"" + obfuscatable_name + "\",\""  + obfuscated_name[obfuscatable_name] + "\""
            if obfuscatable_name != obfuscated_names_by_size[-1]: suffix = suffix + ","
        suffix = suffix + "]\n"
        print "suffix: " + suffix
        global_allocs = "var " + obfuscated_name["HEAP32"] + " = HEAP32;\n" + global_allocs
        pre = pre[:global_allocs_start] + global_allocs + pre[global_allocs_end:]


    # Pick where to split into chunks, so that (1) they do not oom in node/uglify, and (2) we can run them in parallel
    # If we have metadata, we split only the generated code, and save the pre and post on the side (and do not optimize them)
    parts = map(lambda part: part, js.split('\n}\n'))
    funcs = []
    for i in range(len(parts)):
        func = parts[i]
        if i < len(parts)-1: func += '\n}\n' # last part needs no }
        m = func_sig.search(func)
        if m:
            ident = m.group(1)
        else:
            if suffix: continue # ignore whitespace
            ident = 'anon_%d' % i
        funcs.append((ident, func))
    parts = None
    total_size = len(js)
    js = None

    chunks = shared.JCache.chunkify(funcs, BEST_JS_PROCESS_SIZE, 'jsopt' if jcache else None)
    if DEBUG: print >>sys.stderr, 'len chunks:' , len(chunks)

    if jcache:
        # load chunks from cache where we can # TODO: ignore small chunks
        cached_outputs = []
        def load_from_cache(chunk):
            keys = [chunk]
            shortkey = shared.JCache.get_shortkey(keys) # TODO: share shortkeys with later code
            out = shared.JCache.get(shortkey, keys)
            if out:
                cached_outputs.append(out)
                return False
            return True
        chunks = filter(load_from_cache, chunks)
        if len(cached_outputs) > 0:
            if DEBUG: print >> sys.stderr, '  loading %d jsfuncchunks from jcache' % len(cached_outputs)
        else:
            cached_outputs = []

    if len(chunks) > 0:
        if DEBUG: print >>sys.stderr, 'here'
        def write_chunk(chunk, i):
            temp_file = temp_files.get('.jsfunc_%d.ll' % i).name
            f = open(temp_file, 'w')
            if obfuscate_globals:
                print "About to obfuscate names in a chunk\n"
                name_num = 0
                total_names = len(obfuscated_names_by_size)
                for obfuscatable_name in obfuscated_names_by_size:
        #chunk = chunk.replace(obfuscatable_name, obfuscated_name[obfuscatable_name])
                    name_num = name_num + 1
                    if (name_num % 100 == 0):
                        print "Done " + str(name_num) + " of " + str(total_names)
                print "... done\n"
            f.write(chunk)
            f.write(suffix)
            f.close()
            return temp_file
        filenames = [write_chunk(chunks[i], i) for i in range(len(chunks))]
        if DEBUG: print >>sys.stderr, 'filenames: ' , filenames
    else:
        filenames = []

    if len(filenames) > 0:
        # XXX Use '--nocrankshaft' to disable crankshaft to work around v8 bug 1895, needed for older v8/node (node 0.6.8+ should be ok)
        commands = map(lambda filename: [js_engine, JS_OPTIMIZER, filename, 'noPrintMetadata'] + passes, filenames)
        print >>sys.stderr, 'commands: ' , commands

        cores = min(multiprocessing.cpu_count(), filenames)
        cores = 1 # TODO - remove this
        if len(chunks) > 1 and cores >= 2:
            # We can parallelize
            if DEBUG: print >> sys.stderr, 'splitting up js optimization into %d chunks, using %d cores  (total: %.2f MB)' % (len(chunks), cores, total_size/(1024*1024.))
            pool = multiprocessing.Pool(processes=cores)
            filenames = pool.map(run_on_chunk, commands, chunksize=1)
        else:
            # We can't parallize, but still break into chunks to avoid uglify/node memory issues
            if len(chunks) > 1 and DEBUG: print >> sys.stderr, 'splitting up js optimization into %d chunks' % (len(chunks))
            filenames = [run_on_chunk(command) for command in commands]
    else:
        if DEBUG: print >>sys.stderr, 'no file names'
        filenames = []

    filename += '.jo.js'
    f = open(filename, 'w')
    f.write(pre);
    for out_file in filenames:
        f.write(open(out_file).read())
        f.write('\n')
    if jcache:
        for cached in cached_outputs:
            f.write(cached); # TODO: preserve order
            f.write('\n')
    f.write(post);
    # No need to write suffix: if there was one, it is inside post which exists when suffix is there
    f.write('\n')
    f.close()

    if jcache:
        # save chunks to cache
        for i in range(len(chunks)):
            chunk = chunks[i]
            keys = [chunk]
            shortkey = shared.JCache.get_shortkey(keys)
            shared.JCache.set(shortkey, keys, open(filenames[i]).read())
        if DEBUG and len(chunks) > 0: print >> sys.stderr, '  saving %d jsfuncchunks to jcache' % len(chunks)

    return filename
