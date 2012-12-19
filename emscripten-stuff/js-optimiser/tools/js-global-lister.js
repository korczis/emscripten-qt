  // Expose functionality in the same simple way that the shells work
  // Note that we pollute the global namespace here, otherwise we break in node
  print = function(x) {
    process['stdout'].write(x + '\n');
  };
  printErr = function(x) {
    process['stderr'].write(x + '\n');
  };

  var nodeFS = require('fs');
  var nodePath = require('path');

  if (!nodeFS.existsSync) {
    nodeFS.existsSync = function(path) {
      try {
        return !!nodeFS.readFileSync(path);
      } catch(e) {
        return false;
      }
    }
  }

  function find(filename) {
    var prefixes = [nodePath.join(__dirname, '..', 'src'), process.cwd()];
    for (var i = 0; i < prefixes.length; ++i) {
      var combined = nodePath.join(prefixes[i], filename);
      if (nodeFS.existsSync(combined)) {
        return combined;
      }
    }
    return filename;
  }

  read = function(filename) {
    var absolute = find(filename);
    return nodeFS['readFileSync'](absolute).toString();
  };

  load = function(f) {
    globalEval(read(f));
  };

  arguments_ = process['argv'].slice(2);
var uglify = require('../tools/eliminator/node_modules/uglify-js'); 
var fs = require('fs'); 
var path = require('path'); 
 
// Load some modules 
 
load('utility.js'); 

function srcToAst(src) {
  return uglify.parser.parse(src);
}

function astToSrc(ast, compress) {
    return uglify.uglify.gen_code(ast, {
    ascii_only: true,
    beautify: !compress,
    indent_level: 2
  });
}
function globalEval(x) {
  eval.call(null, x);
}


var src = read(arguments_[0]);
var ast = srcToAst(src);
print = function(x) {
    process['stdout'].write(x + '\n');
  };
  printErr = function(x) {
    process['stderr'].write(x + '\n');
  };

var toplevel = ast[1];
for (var i = 0; i < toplevel.length; i++)
{
	if (toplevel[i][0] == "var")
	{
		var num_declared_variables = toplevel[i][1].length; 
		for (var declaration_num = 0; declaration_num < num_declared_variables; declaration_num++)
		{
			print(toplevel[i][1][declaration_num][0]);
		}
	}
	else if (toplevel[i][0] == "defun")
	{
			print (toplevel[i][1]);
	}
}
