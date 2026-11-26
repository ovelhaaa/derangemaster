// src/wasm/rangemaster_dsp.js
var Module = (() => {
  var _scriptName = import.meta.url;
  return async function(moduleArg = {}) {
    var moduleRtn;
    var Module2 = moduleArg;
    var readyPromiseResolve, readyPromiseReject;
    var readyPromise = new Promise((resolve, reject) => {
      readyPromiseResolve = resolve;
      readyPromiseReject = reject;
    });
    var ENVIRONMENT_IS_WEB = typeof window == "object";
    var ENVIRONMENT_IS_WORKER = typeof importScripts == "function";
    var ENVIRONMENT_IS_NODE = typeof process == "object" && typeof process.versions == "object" && typeof process.versions.node == "string";
    if (ENVIRONMENT_IS_NODE) {
      const { createRequire } = await import("module");
      var require2 = createRequire(import.meta.url);
    }
    var moduleOverrides = Object.assign({}, Module2);
    var arguments_ = [];
    var thisProgram = "./this.program";
    var quit_ = (status, toThrow) => {
      throw toThrow;
    };
    var scriptDirectory = "";
    function locateFile(path) {
      if (Module2["locateFile"]) {
        return Module2["locateFile"](path, scriptDirectory);
      }
      return scriptDirectory + path;
    }
    var readAsync, readBinary;
    if (ENVIRONMENT_IS_NODE) {
      var fs = require2("fs");
      var nodePath = require2("path");
      scriptDirectory = require2("url").fileURLToPath(new URL("./", import.meta.url));
      readBinary = (filename) => {
        filename = isFileURI(filename) ? new URL(filename) : nodePath.normalize(filename);
        var ret = fs.readFileSync(filename);
        return ret;
      };
      readAsync = (filename, binary = true) => {
        filename = isFileURI(filename) ? new URL(filename) : nodePath.normalize(filename);
        return new Promise((resolve, reject) => {
          fs.readFile(filename, binary ? void 0 : "utf8", (err2, data) => {
            if (err2) reject(err2);
            else resolve(binary ? data.buffer : data);
          });
        });
      };
      if (!Module2["thisProgram"] && process.argv.length > 1) {
        thisProgram = process.argv[1].replace(/\\/g, "/");
      }
      arguments_ = process.argv.slice(2);
      quit_ = (status, toThrow) => {
        process.exitCode = status;
        throw toThrow;
      };
    } else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
      if (ENVIRONMENT_IS_WORKER) {
        scriptDirectory = self.location.href;
      } else if (typeof document != "undefined" && document.currentScript) {
        scriptDirectory = document.currentScript.src;
      }
      if (_scriptName) {
        scriptDirectory = _scriptName;
      }
      if (scriptDirectory.startsWith("blob:")) {
        scriptDirectory = "";
      } else {
        scriptDirectory = scriptDirectory.substr(0, scriptDirectory.replace(/[?#].*/, "").lastIndexOf("/") + 1);
      }
      {
        if (ENVIRONMENT_IS_WORKER) {
          readBinary = (url) => {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", url, false);
            xhr.responseType = "arraybuffer";
            xhr.send(null);
            return new Uint8Array(xhr.response);
          };
        }
        readAsync = (url) => fetch(url, { credentials: "same-origin" }).then((response) => {
          if (response.ok) {
            return response.arrayBuffer();
          }
          return Promise.reject(new Error(response.status + " : " + response.url));
        });
      }
    } else {
    }
    var out = Module2["print"] || console.log.bind(console);
    var err = Module2["printErr"] || console.error.bind(console);
    Object.assign(Module2, moduleOverrides);
    moduleOverrides = null;
    if (Module2["arguments"]) arguments_ = Module2["arguments"];
    if (Module2["thisProgram"]) thisProgram = Module2["thisProgram"];
    if (Module2["quit"]) quit_ = Module2["quit"];
    var wasmBinary;
    if (Module2["wasmBinary"]) wasmBinary = Module2["wasmBinary"];
    var wasmMemory;
    var ABORT = false;
    var EXITSTATUS;
    var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;
    function updateMemoryViews() {
      var b = wasmMemory.buffer;
      Module2["HEAP8"] = HEAP8 = new Int8Array(b);
      Module2["HEAP16"] = HEAP16 = new Int16Array(b);
      Module2["HEAPU8"] = HEAPU8 = new Uint8Array(b);
      Module2["HEAPU16"] = HEAPU16 = new Uint16Array(b);
      Module2["HEAP32"] = HEAP32 = new Int32Array(b);
      Module2["HEAPU32"] = HEAPU32 = new Uint32Array(b);
      Module2["HEAPF32"] = HEAPF32 = new Float32Array(b);
      Module2["HEAPF64"] = HEAPF64 = new Float64Array(b);
    }
    var __ATPRERUN__ = [];
    var __ATINIT__ = [];
    var __ATPOSTRUN__ = [];
    var runtimeInitialized = false;
    function preRun() {
      if (Module2["preRun"]) {
        if (typeof Module2["preRun"] == "function") Module2["preRun"] = [Module2["preRun"]];
        while (Module2["preRun"].length) {
          addOnPreRun(Module2["preRun"].shift());
        }
      }
      callRuntimeCallbacks(__ATPRERUN__);
    }
    function initRuntime() {
      runtimeInitialized = true;
      callRuntimeCallbacks(__ATINIT__);
    }
    function postRun() {
      if (Module2["postRun"]) {
        if (typeof Module2["postRun"] == "function") Module2["postRun"] = [Module2["postRun"]];
        while (Module2["postRun"].length) {
          addOnPostRun(Module2["postRun"].shift());
        }
      }
      callRuntimeCallbacks(__ATPOSTRUN__);
    }
    function addOnPreRun(cb) {
      __ATPRERUN__.unshift(cb);
    }
    function addOnInit(cb) {
      __ATINIT__.unshift(cb);
    }
    function addOnPostRun(cb) {
      __ATPOSTRUN__.unshift(cb);
    }
    var runDependencies = 0;
    var runDependencyWatcher = null;
    var dependenciesFulfilled = null;
    function addRunDependency(id) {
      runDependencies++;
      Module2["monitorRunDependencies"]?.(runDependencies);
    }
    function removeRunDependency(id) {
      runDependencies--;
      Module2["monitorRunDependencies"]?.(runDependencies);
      if (runDependencies == 0) {
        if (runDependencyWatcher !== null) {
          clearInterval(runDependencyWatcher);
          runDependencyWatcher = null;
        }
        if (dependenciesFulfilled) {
          var callback = dependenciesFulfilled;
          dependenciesFulfilled = null;
          callback();
        }
      }
    }
    function abort(what) {
      Module2["onAbort"]?.(what);
      what = "Aborted(" + what + ")";
      err(what);
      ABORT = true;
      EXITSTATUS = 1;
      what += ". Build with -sASSERTIONS for more info.";
      var e = new WebAssembly.RuntimeError(what);
      readyPromiseReject(e);
      throw e;
    }
    var dataURIPrefix = "data:application/octet-stream;base64,";
    var isDataURI = (filename) => filename.startsWith(dataURIPrefix);
    var isFileURI = (filename) => filename.startsWith("file://");
    function findWasmBinary() {
      if (Module2["locateFile"]) {
        var f = "rangemaster_dsp.wasm";
        if (!isDataURI(f)) {
          return locateFile(f);
        }
        return f;
      }
      return new URL("rangemaster_dsp.wasm", import.meta.url).href;
    }
    var wasmBinaryFile;
    function getBinarySync(file) {
      if (file == wasmBinaryFile && wasmBinary) {
        return new Uint8Array(wasmBinary);
      }
      if (readBinary) {
        return readBinary(file);
      }
      throw "both async and sync fetching of the wasm failed";
    }
    function getBinaryPromise(binaryFile) {
      if (!wasmBinary) {
        return readAsync(binaryFile).then((response) => new Uint8Array(response), () => getBinarySync(binaryFile));
      }
      return Promise.resolve().then(() => getBinarySync(binaryFile));
    }
    function instantiateArrayBuffer(binaryFile, imports, receiver) {
      return getBinaryPromise(binaryFile).then((binary) => WebAssembly.instantiate(binary, imports)).then(receiver, (reason) => {
        err(`failed to asynchronously prepare wasm: ${reason}`);
        abort(reason);
      });
    }
    function instantiateAsync(binary, binaryFile, imports, callback) {
      if (!binary && typeof WebAssembly.instantiateStreaming == "function" && !isDataURI(binaryFile) && !ENVIRONMENT_IS_NODE && typeof fetch == "function") {
        return fetch(binaryFile, { credentials: "same-origin" }).then((response) => {
          var result = WebAssembly.instantiateStreaming(response, imports);
          return result.then(callback, function(reason) {
            err(`wasm streaming compile failed: ${reason}`);
            err("falling back to ArrayBuffer instantiation");
            return instantiateArrayBuffer(binaryFile, imports, callback);
          });
        });
      }
      return instantiateArrayBuffer(binaryFile, imports, callback);
    }
    function getWasmImports() {
      return { a: wasmImports };
    }
    function createWasm() {
      var info = getWasmImports();
      function receiveInstance(instance, module) {
        wasmExports = instance.exports;
        wasmMemory = wasmExports["c"];
        updateMemoryViews();
        addOnInit(wasmExports["d"]);
        removeRunDependency("wasm-instantiate");
        return wasmExports;
      }
      addRunDependency("wasm-instantiate");
      function receiveInstantiationResult(result) {
        receiveInstance(result["instance"]);
      }
      if (Module2["instantiateWasm"]) {
        try {
          return Module2["instantiateWasm"](info, receiveInstance);
        } catch (e) {
          err(`Module.instantiateWasm callback failed with error: ${e}`);
          readyPromiseReject(e);
        }
      }
      if (!wasmBinaryFile) wasmBinaryFile = findWasmBinary();
      instantiateAsync(wasmBinary, wasmBinaryFile, info, receiveInstantiationResult).catch(readyPromiseReject);
      return {};
    }
    var callRuntimeCallbacks = (callbacks) => {
      while (callbacks.length > 0) {
        callbacks.shift()(Module2);
      }
    };
    var noExitRuntime = Module2["noExitRuntime"] || true;
    var stackRestore = (val) => __emscripten_stack_restore(val);
    var stackSave = () => _emscripten_stack_get_current();
    var __emscripten_memcpy_js = (dest, src, num) => HEAPU8.copyWithin(dest, src, src + num);
    var abortOnCannotGrowMemory = (requestedSize) => {
      abort("OOM");
    };
    var _emscripten_resize_heap = (requestedSize) => {
      var oldSize = HEAPU8.length;
      requestedSize >>>= 0;
      abortOnCannotGrowMemory(requestedSize);
    };
    var getCFunc = (ident) => {
      var func = Module2["_" + ident];
      return func;
    };
    var writeArrayToMemory = (array, buffer) => {
      HEAP8.set(array, buffer);
    };
    var lengthBytesUTF8 = (str) => {
      var len = 0;
      for (var i = 0; i < str.length; ++i) {
        var c = str.charCodeAt(i);
        if (c <= 127) {
          len++;
        } else if (c <= 2047) {
          len += 2;
        } else if (c >= 55296 && c <= 57343) {
          len += 4;
          ++i;
        } else {
          len += 3;
        }
      }
      return len;
    };
    var stringToUTF8Array = (str, heap, outIdx, maxBytesToWrite) => {
      if (!(maxBytesToWrite > 0)) return 0;
      var startIdx = outIdx;
      var endIdx = outIdx + maxBytesToWrite - 1;
      for (var i = 0; i < str.length; ++i) {
        var u = str.charCodeAt(i);
        if (u >= 55296 && u <= 57343) {
          var u1 = str.charCodeAt(++i);
          u = 65536 + ((u & 1023) << 10) | u1 & 1023;
        }
        if (u <= 127) {
          if (outIdx >= endIdx) break;
          heap[outIdx++] = u;
        } else if (u <= 2047) {
          if (outIdx + 1 >= endIdx) break;
          heap[outIdx++] = 192 | u >> 6;
          heap[outIdx++] = 128 | u & 63;
        } else if (u <= 65535) {
          if (outIdx + 2 >= endIdx) break;
          heap[outIdx++] = 224 | u >> 12;
          heap[outIdx++] = 128 | u >> 6 & 63;
          heap[outIdx++] = 128 | u & 63;
        } else {
          if (outIdx + 3 >= endIdx) break;
          heap[outIdx++] = 240 | u >> 18;
          heap[outIdx++] = 128 | u >> 12 & 63;
          heap[outIdx++] = 128 | u >> 6 & 63;
          heap[outIdx++] = 128 | u & 63;
        }
      }
      heap[outIdx] = 0;
      return outIdx - startIdx;
    };
    var stringToUTF8 = (str, outPtr, maxBytesToWrite) => stringToUTF8Array(str, HEAPU8, outPtr, maxBytesToWrite);
    var stackAlloc = (sz) => __emscripten_stack_alloc(sz);
    var stringToUTF8OnStack = (str) => {
      var size = lengthBytesUTF8(str) + 1;
      var ret = stackAlloc(size);
      stringToUTF8(str, ret, size);
      return ret;
    };
    var UTF8Decoder = typeof TextDecoder != "undefined" ? new TextDecoder() : void 0;
    var UTF8ArrayToString = (heapOrArray, idx, maxBytesToRead) => {
      var endIdx = idx + maxBytesToRead;
      var endPtr = idx;
      while (heapOrArray[endPtr] && !(endPtr >= endIdx)) ++endPtr;
      if (endPtr - idx > 16 && heapOrArray.buffer && UTF8Decoder) {
        return UTF8Decoder.decode(heapOrArray.subarray(idx, endPtr));
      }
      var str = "";
      while (idx < endPtr) {
        var u0 = heapOrArray[idx++];
        if (!(u0 & 128)) {
          str += String.fromCharCode(u0);
          continue;
        }
        var u1 = heapOrArray[idx++] & 63;
        if ((u0 & 224) == 192) {
          str += String.fromCharCode((u0 & 31) << 6 | u1);
          continue;
        }
        var u2 = heapOrArray[idx++] & 63;
        if ((u0 & 240) == 224) {
          u0 = (u0 & 15) << 12 | u1 << 6 | u2;
        } else {
          u0 = (u0 & 7) << 18 | u1 << 12 | u2 << 6 | heapOrArray[idx++] & 63;
        }
        if (u0 < 65536) {
          str += String.fromCharCode(u0);
        } else {
          var ch = u0 - 65536;
          str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023);
        }
      }
      return str;
    };
    var UTF8ToString = (ptr, maxBytesToRead) => ptr ? UTF8ArrayToString(HEAPU8, ptr, maxBytesToRead) : "";
    var ccall = (ident, returnType, argTypes, args, opts) => {
      var toC = { string: (str) => {
        var ret2 = 0;
        if (str !== null && str !== void 0 && str !== 0) {
          ret2 = stringToUTF8OnStack(str);
        }
        return ret2;
      }, array: (arr) => {
        var ret2 = stackAlloc(arr.length);
        writeArrayToMemory(arr, ret2);
        return ret2;
      } };
      function convertReturnValue(ret2) {
        if (returnType === "string") {
          return UTF8ToString(ret2);
        }
        if (returnType === "boolean") return Boolean(ret2);
        return ret2;
      }
      var func = getCFunc(ident);
      var cArgs = [];
      var stack = 0;
      if (args) {
        for (var i = 0; i < args.length; i++) {
          var converter = toC[argTypes[i]];
          if (converter) {
            if (stack === 0) stack = stackSave();
            cArgs[i] = converter(args[i]);
          } else {
            cArgs[i] = args[i];
          }
        }
      }
      var ret = func(...cArgs);
      function onDone(ret2) {
        if (stack !== 0) stackRestore(stack);
        return convertReturnValue(ret2);
      }
      ret = onDone(ret);
      return ret;
    };
    var cwrap = (ident, returnType, argTypes, opts) => {
      var numericArgs = !argTypes || argTypes.every((type) => type === "number" || type === "boolean");
      var numericRet = returnType !== "string";
      if (numericRet && numericArgs && !opts) {
        return getCFunc(ident);
      }
      return (...args) => ccall(ident, returnType, argTypes, args, opts);
    };
    var wasmImports = { b: __emscripten_memcpy_js, a: _emscripten_resize_heap };
    var wasmExports = createWasm();
    var ___wasm_call_ctors = () => (___wasm_call_ctors = wasmExports["d"])();
    var _rangemaster_create = Module2["_rangemaster_create"] = () => (_rangemaster_create = Module2["_rangemaster_create"] = wasmExports["e"])();
    var _rangemaster_destroy = Module2["_rangemaster_destroy"] = (a0) => (_rangemaster_destroy = Module2["_rangemaster_destroy"] = wasmExports["f"])(a0);
    var _rangemaster_prepare = Module2["_rangemaster_prepare"] = (a0, a1, a2) => (_rangemaster_prepare = Module2["_rangemaster_prepare"] = wasmExports["g"])(a0, a1, a2);
    var _rangemaster_reset = Module2["_rangemaster_reset"] = (a0) => (_rangemaster_reset = Module2["_rangemaster_reset"] = wasmExports["h"])(a0);
    var _rangemaster_reset_diagnostics = Module2["_rangemaster_reset_diagnostics"] = (a0) => (_rangemaster_reset_diagnostics = Module2["_rangemaster_reset_diagnostics"] = wasmExports["i"])(a0);
    var _rangemaster_process_f64 = Module2["_rangemaster_process_f64"] = (a0, a1, a2, a3) => (_rangemaster_process_f64 = Module2["_rangemaster_process_f64"] = wasmExports["j"])(a0, a1, a2, a3);
    var _rangemaster_process_f32 = Module2["_rangemaster_process_f32"] = (a0, a1, a2, a3) => (_rangemaster_process_f32 = Module2["_rangemaster_process_f32"] = wasmExports["k"])(a0, a1, a2, a3);
    var _rangemaster_set_engine = Module2["_rangemaster_set_engine"] = (a0, a1) => (_rangemaster_set_engine = Module2["_rangemaster_set_engine"] = wasmExports["l"])(a0, a1);
    var _rangemaster_set_transistor = Module2["_rangemaster_set_transistor"] = (a0, a1) => (_rangemaster_set_transistor = Module2["_rangemaster_set_transistor"] = wasmExports["m"])(a0, a1);
    var _rangemaster_set_transistor_by_name = Module2["_rangemaster_set_transistor_by_name"] = (a0, a1) => (_rangemaster_set_transistor_by_name = Module2["_rangemaster_set_transistor_by_name"] = wasmExports["n"])(a0, a1);
    var _rangemaster_set_oversampling = Module2["_rangemaster_set_oversampling"] = (a0, a1) => (_rangemaster_set_oversampling = Module2["_rangemaster_set_oversampling"] = wasmExports["o"])(a0, a1);
    var _rangemaster_set_boost = Module2["_rangemaster_set_boost"] = (a0, a1) => (_rangemaster_set_boost = Module2["_rangemaster_set_boost"] = wasmExports["p"])(a0, a1);
    var _rangemaster_set_pot_taper = Module2["_rangemaster_set_pot_taper"] = (a0, a1) => (_rangemaster_set_pot_taper = Module2["_rangemaster_set_pot_taper"] = wasmExports["q"])(a0, a1);
    var _rangemaster_set_boost_linear_approx = Module2["_rangemaster_set_boost_linear_approx"] = (a0, a1) => (_rangemaster_set_boost_linear_approx = Module2["_rangemaster_set_boost_linear_approx"] = wasmExports["r"])(a0, a1);
    var _rangemaster_set_input_volts = Module2["_rangemaster_set_input_volts"] = (a0, a1) => (_rangemaster_set_input_volts = Module2["_rangemaster_set_input_volts"] = wasmExports["s"])(a0, a1);
    var _rangemaster_set_output_ref_volts = Module2["_rangemaster_set_output_ref_volts"] = (a0, a1) => (_rangemaster_set_output_ref_volts = Module2["_rangemaster_set_output_ref_volts"] = wasmExports["t"])(a0, a1);
    var _rangemaster_set_output_trim_db = Module2["_rangemaster_set_output_trim_db"] = (a0, a1) => (_rangemaster_set_output_trim_db = Module2["_rangemaster_set_output_trim_db"] = wasmExports["u"])(a0, a1);
    var _rangemaster_set_range_nf = Module2["_rangemaster_set_range_nf"] = (a0, a1) => (_rangemaster_set_range_nf = Module2["_rangemaster_set_range_nf"] = wasmExports["v"])(a0, a1);
    var _rangemaster_set_source_ohms = Module2["_rangemaster_set_source_ohms"] = (a0, a1) => (_rangemaster_set_source_ohms = Module2["_rangemaster_set_source_ohms"] = wasmExports["w"])(a0, a1);
    var _rangemaster_set_load_ohms = Module2["_rangemaster_set_load_ohms"] = (a0, a1) => (_rangemaster_set_load_ohms = Module2["_rangemaster_set_load_ohms"] = wasmExports["x"])(a0, a1);
    var _rangemaster_set_supply_volts = Module2["_rangemaster_set_supply_volts"] = (a0, a1) => (_rangemaster_set_supply_volts = Module2["_rangemaster_set_supply_volts"] = wasmExports["y"])(a0, a1);
    var _rangemaster_set_bias_trim = Module2["_rangemaster_set_bias_trim"] = (a0, a1) => (_rangemaster_set_bias_trim = Module2["_rangemaster_set_bias_trim"] = wasmExports["z"])(a0, a1);
    var _rangemaster_set_solver_profile = Module2["_rangemaster_set_solver_profile"] = (a0, a1) => (_rangemaster_set_solver_profile = Module2["_rangemaster_set_solver_profile"] = wasmExports["A"])(a0, a1);
    var _rangemaster_set_gp_ablation = Module2["_rangemaster_set_gp_ablation"] = (a0, a1) => (_rangemaster_set_gp_ablation = Module2["_rangemaster_set_gp_ablation"] = wasmExports["B"])(a0, a1);
    var _rangemaster_get_sample_rate = Module2["_rangemaster_get_sample_rate"] = (a0) => (_rangemaster_get_sample_rate = Module2["_rangemaster_get_sample_rate"] = wasmExports["C"])(a0);
    var _rangemaster_get_internal_sample_rate = Module2["_rangemaster_get_internal_sample_rate"] = (a0) => (_rangemaster_get_internal_sample_rate = Module2["_rangemaster_get_internal_sample_rate"] = wasmExports["D"])(a0);
    var _rangemaster_get_latency_samples = Module2["_rangemaster_get_latency_samples"] = (a0) => (_rangemaster_get_latency_samples = Module2["_rangemaster_get_latency_samples"] = wasmExports["E"])(a0);
    var _rangemaster_get_matrix_rebuild_count = Module2["_rangemaster_get_matrix_rebuild_count"] = (a0) => (_rangemaster_get_matrix_rebuild_count = Module2["_rangemaster_get_matrix_rebuild_count"] = wasmExports["F"])(a0);
    var _rangemaster_get_conditioning = Module2["_rangemaster_get_conditioning"] = (a0, a1, a2, a3, a4) => (_rangemaster_get_conditioning = Module2["_rangemaster_get_conditioning"] = wasmExports["G"])(a0, a1, a2, a3, a4);
    var _rangemaster_get_diagnostics = Module2["_rangemaster_get_diagnostics"] = (a0, a1) => (_rangemaster_get_diagnostics = Module2["_rangemaster_get_diagnostics"] = wasmExports["H"])(a0, a1);
    var _rangemaster_get_dc_operating_point = Module2["_rangemaster_get_dc_operating_point"] = (a0, a1) => (_rangemaster_get_dc_operating_point = Module2["_rangemaster_get_dc_operating_point"] = wasmExports["I"])(a0, a1);
    var _malloc = Module2["_malloc"] = (a0) => (_malloc = Module2["_malloc"] = wasmExports["K"])(a0);
    var _free = Module2["_free"] = (a0) => (_free = Module2["_free"] = wasmExports["L"])(a0);
    var __emscripten_stack_restore = (a0) => (__emscripten_stack_restore = wasmExports["M"])(a0);
    var __emscripten_stack_alloc = (a0) => (__emscripten_stack_alloc = wasmExports["N"])(a0);
    var _emscripten_stack_get_current = () => (_emscripten_stack_get_current = wasmExports["O"])();
    Module2["ccall"] = ccall;
    Module2["cwrap"] = cwrap;
    var calledRun;
    dependenciesFulfilled = function runCaller() {
      if (!calledRun) run();
      if (!calledRun) dependenciesFulfilled = runCaller;
    };
    function run() {
      if (runDependencies > 0) {
        return;
      }
      preRun();
      if (runDependencies > 0) {
        return;
      }
      function doRun() {
        if (calledRun) return;
        calledRun = true;
        Module2["calledRun"] = true;
        if (ABORT) return;
        initRuntime();
        readyPromiseResolve(Module2);
        Module2["onRuntimeInitialized"]?.();
        postRun();
      }
      if (Module2["setStatus"]) {
        Module2["setStatus"]("Running...");
        setTimeout(function() {
          setTimeout(function() {
            Module2["setStatus"]("");
          }, 1);
          doRun();
        }, 1);
      } else {
        doRun();
      }
    }
    if (Module2["preInit"]) {
      if (typeof Module2["preInit"] == "function") Module2["preInit"] = [Module2["preInit"]];
      while (Module2["preInit"].length > 0) {
        Module2["preInit"].pop()();
      }
    }
    run();
    moduleRtn = readyPromise;
    return moduleRtn;
  };
})();
var rangemaster_dsp_default = Module;

// src/state/types.ts
var ENGINE_META = {
  "em-dc": {
    id: 0,
    label: "EM-DC (Engine A)",
    shortLabel: "EM-DC",
    description: "Static PNP Ebers-Moll BJT inside 3-capacitor Nodal DK circuit."
  },
  "em-ac": {
    id: 1,
    label: "EM-AC (Engine B)",
    shortLabel: "EM-AC",
    description: "Ebers-Moll with datasheet junction capacitances Ceb/Ccb (Miller roll-off)."
  },
  "gp-dc": {
    id: 2,
    label: "GP-DC (Engine C)",
    shortLabel: "GP-DC",
    description: "Static Gummel-Poon (Early, high-injection, leakage, Rb/Re/Rc)."
  },
  "gp-ac": {
    id: 3,
    label: "GP-AC (Engine D)",
    shortLabel: "GP-AC",
    description: "Full Gummel-Poon + junction capacitances (up to 5 state capacitors)."
  },
  "calibrated-oc44": {
    id: 4,
    label: "Holmes Calibrated (Engine E)",
    shortLabel: "Cal-OC44",
    description: "Experimental: Holmes (2019) Table 6.2 end-to-end calibrated set (biases near saturation; isolated from nominal profiles).",
    experimental: true
  }
};
function ablationToMask(f) {
  return (f.enableJunctionCaps ? 1 : 0) | (f.enableEarlyEffect ? 2 : 0) | (f.enableLeakageCurrents ? 4 : 0) | (f.enableHighCurrentRolloff ? 8 : 0) | (f.enableTerminalResistances ? 16 : 0);
}

// src/state/transistors.ts
var TRANSISTOR_CATALOG = [
  {
    index: 0,
    id: "oc44-01",
    label: "OC44 #1",
    partNumber: "Mullard OC44 (#1)",
    family: "OC44",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "Thesis pp. 71, 80 (PDF pp. 95, 104); Is=1.423 uA, betaF=307, Vaf=8.17 V",
    extractionMethod: "Keithley 2602B SMU pulsed I-V + Nelder-Mead/interior-point fit; OC44 datasheet Ceb/Ccb",
    hasCeb: true,
    hasCcb: true,
    cebPf: 410,
    ccbPf: 10.5,
    nominalVcVolts: -6.596
  },
  {
    index: 1,
    id: "oc44-02",
    label: "OC44 #2",
    partNumber: "Mullard OC44 (#2*)",
    family: "OC44",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "Best-fit VA reference specimen; Is=2.245 uA, betaF=283.5, Vaf=6.86 V",
    extractionMethod: "Keithley 2602B SMU pulsed I-V + optimization fit; OC44 datasheet Ceb/Ccb",
    hasCeb: true,
    hasCcb: true,
    cebPf: 410,
    ccbPf: 10.5,
    nominalVcVolts: -6.611
  },
  {
    index: 2,
    id: "oc44-03",
    label: "OC44 #3",
    partNumber: "Mullard OC44 (#3)",
    family: "OC44",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "Higher knee current specimen; Is=2.867 uA, betaF=226.2, Vaf=8.79 V",
    extractionMethod: "Keithley 2602B SMU pulsed I-V + optimization fit; OC44 datasheet Ceb/Ccb",
    hasCeb: true,
    hasCcb: true,
    cebPf: 410,
    ccbPf: 10.5,
    nominalVcVolts: -6.556
  },
  {
    index: 3,
    id: "oc44-dafx17",
    label: "OC44 DAFx17",
    partNumber: "Mullard OC44 (DAFx-17 Table 1)",
    family: "OC44",
    citation: "Holmes, Holters & van Walstijn (2017) DAFx-17 Table 1",
    detail: "Published DAFx-17 Fig. 10 reference specimen; Ccb=10 pF",
    extractionMethod: "Keithley 2602B SMU pulsed I-V + multi-stage optimization; OC44 datasheet",
    hasCeb: true,
    hasCcb: true,
    cebPf: 410,
    ccbPf: 10,
    nominalVcVolts: -6.596
  },
  {
    index: 4,
    id: "ac128-01",
    label: "AC128 #1",
    partNumber: "AC128 (#1*)",
    family: "AC128",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "Best-fit AC128 specimen; higher Is (27.8 uA) & leakage; Ceb absent in datasheet",
    extractionMethod: "Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)",
    hasCeb: false,
    hasCcb: true,
    cebPf: 0,
    ccbPf: 100,
    nominalVcVolts: -6.336
  },
  {
    index: 5,
    id: "ac128-02",
    label: "AC128 #2",
    partNumber: "AC128 (#2)",
    family: "AC128",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "Is=31.67 uA, betaF=125.7, Vaf=61.24 V; Ceb absent in datasheet",
    extractionMethod: "Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)",
    hasCeb: false,
    hasCcb: true,
    cebPf: 0,
    ccbPf: 100,
    nominalVcVolts: -6.556
  },
  {
    index: 6,
    id: "ac128-03",
    label: "AC128 #3",
    partNumber: "AC128 (#3)",
    family: "AC128",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
    detail: "High reverse CB leakage (negative quiescent Ib); Ceb absent in datasheet",
    extractionMethod: "Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)",
    hasCeb: false,
    hasCcb: true,
    cebPf: 0,
    ccbPf: 100,
    nominalVcVolts: -6.116
  },
  {
    index: 7,
    id: "ac128-04",
    label: "AC128 #4",
    partNumber: "AC128 (#4)",
    family: "AC128",
    citation: "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3 (DAFx-17 Table 1)",
    detail: "Is=20.66 uA, betaF=229.6; Ceb absent in datasheet",
    extractionMethod: "Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)",
    hasCeb: false,
    hasCcb: true,
    cebPf: 0,
    ccbPf: 100,
    nominalVcVolts: -6.166
  }
];
function getTransistorMeta(id) {
  return TRANSISTOR_CATALOG.find((t) => t.id === id) ?? TRANSISTOR_CATALOG[1];
}

// src/wasm/rangemasterWasm.ts
async function instantiateRangemasterWasm(wasmBinary) {
  const opts = {};
  if (wasmBinary) {
    opts.wasmBinary = new Uint8Array(wasmBinary);
  }
  const mod = await rangemaster_dsp_default(opts);
  return mod;
}
function applyFullConfigToHandle(M, h, cfg) {
  const engineId = ENGINE_META[cfg.engine]?.id ?? 3;
  const txMeta = getTransistorMeta(cfg.transistorId);
  const checks = [
    ["engine", M._rangemaster_set_engine(h, engineId)],
    ["transistor", M._rangemaster_set_transistor(h, txMeta.index)],
    ["oversampling", M._rangemaster_set_oversampling(h, cfg.oversampling)],
    ["gpAblation", M._rangemaster_set_gp_ablation(h, ablationToMask(cfg.gpAblation))],
    ["rangeNf", M._rangemaster_set_range_nf(h, cfg.rangeNf)],
    ["boost", M._rangemaster_set_boost(h, cfg.boost)],
    ["potTaper", M._rangemaster_set_pot_taper(h, cfg.potTaper)],
    ["boostLinearApprox", M._rangemaster_set_boost_linear_approx(h, cfg.boostLinearApprox ? 1 : 0)],
    ["inputVolts", M._rangemaster_set_input_volts(h, cfg.inputVolts)],
    ["outputRefVolts", M._rangemaster_set_output_ref_volts(h, cfg.outputRefVolts)],
    ["outputTrimDb", M._rangemaster_set_output_trim_db(h, cfg.outputTrimDb)],
    ["biasTrim", M._rangemaster_set_bias_trim(h, cfg.biasTrim)],
    ["supplyVolts", M._rangemaster_set_supply_volts(h, cfg.supplyVolts)],
    ["sourceOhms", M._rangemaster_set_source_ohms(h, cfg.sourceOhms)],
    ["loadOhms", M._rangemaster_set_load_ohms(h, cfg.loadOhms)]
  ];
  for (const [name, rc] of checks) {
    if (rc !== 1) {
      return { ok: false, error: `Parameter '${name}' rejected by DSP core` };
    }
  }
  return { ok: true };
}
function readU64(u32, idx32) {
  const lo = u32[idx32] >>> 0;
  const hi = u32[idx32 + 1] >>> 0;
  return hi * 4294967296 + lo;
}
function readDiagnosticsFromPtr(M, diagPtr) {
  const u32 = new Uint32Array(M.HEAPU32.buffer, diagPtr, 44);
  const f64 = new Float64Array(M.HEAPF64.buffer, diagPtr, 22);
  return {
    totalCalls: readU64(u32, 0),
    totalIterations: readU64(u32, 2),
    totalSubIterations: readU64(u32, 4),
    maxIterationsObserved: u32[6] >>> 0,
    meanIterations: f64[4],
    meanSubIterations: f64[5],
    lastResidual: f64[6],
    maxFinalResidual: f64[7],
    recoveryEvents: readU64(u32, 16),
    convergenceFailures: readU64(u32, 18),
    nonFiniteEvents: readU64(u32, 20),
    totalNonlinearEvals: readU64(u32, 22),
    maxNonlinearEvalsPerCall: u32[24] >>> 0,
    totalJacobianEvals: readU64(u32, 26),
    regularizations: readU64(u32, 28),
    backtrackEvents: readU64(u32, 30),
    dcFallbackRecoveries: readU64(u32, 32),
    homotopyRecoveries: readU64(u32, 34),
    budgetExhaustionEvents: readU64(u32, 36),
    domainGuardEvents: readU64(u32, 38),
    stagnationEvents: readU64(u32, 40),
    maxConsecutiveFailures: u32[42] >>> 0
  };
}
function readDcOpFromPtr(M, dcPtr) {
  const f64 = new Float64Array(M.HEAPF64.buffer, dcPtr, 10);
  const i32 = new Int32Array(M.HEAP32.buffer, dcPtr, 22);
  return {
    vb: f64[0],
    ve: f64[1],
    vc: f64[2],
    veb: f64[3],
    vcb: f64[4],
    vec: f64[5],
    ib: f64[6],
    ic: f64[7],
    ie: f64[8],
    powerWatts: f64[9],
    converged: i32[20] === 1
  };
}
function readConditioning(M, h, condPtr) {
  M._rangemaster_get_conditioning(h, condPtr, condPtr + 8, condPtr + 16, condPtr + 24);
  const f64 = new Float64Array(M.HEAPF64.buffer, condPtr, 4);
  return {
    condDyn: f64[0],
    condDc: f64[1],
    resDyn: f64[2],
    resDc: f64[3]
  };
}

// src/state/presets.ts
var NOMINAL_DEFAULT_CONFIG = {
  engine: "gp-ac",
  transistorId: "oc44-02",
  oversampling: 4,
  boost: 1,
  potTaper: 0,
  boostLinearApprox: false,
  rangeNf: 4.7,
  inputVolts: 0.35,
  outputRefVolts: 9,
  outputTrimDb: 0,
  biasTrim: 1,
  supplyVolts: 9,
  sourceOhms: 1e3,
  loadOhms: 1e6,
  gpAblation: {
    enableJunctionCaps: true,
    enableEarlyEffect: true,
    enableLeakageCurrents: true,
    enableHighCurrentRolloff: true,
    enableTerminalResistances: true
  }
};
var PRESET_LIST = [
  {
    id: "original-oc44",
    name: "Original OC44",
    description: "Reference GP-AC + OC44 #2, 4.7 nF Original Range, 100% Boost, 0.35 V/FS.",
    config: { ...NOMINAL_DEFAULT_CONFIG }
  },
  {
    id: "vintage-narrow",
    name: "Vintage Narrow",
    description: "OC44 #1 with 2.2 nF Narrow input capacitor for tight upper-mid focus.",
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: "oc44-01", rangeNf: 2.2 }
  },
  {
    id: "midrange-push",
    name: "Midrange Push",
    description: "10 nF Mid cap, 90% Boost, 0.45 V/FS for thicker vocal midrange.",
    config: { ...NOMINAL_DEFAULT_CONFIG, rangeNf: 10, boost: 0.9, inputVolts: 0.45 }
  },
  {
    id: "wide-range",
    name: "Wide Range",
    description: "22 nF Wide cap, 85% Boost, 0.40 V/FS for fuller low-mid response.",
    config: { ...NOMINAL_DEFAULT_CONFIG, rangeNf: 22, boost: 0.85, inputVolts: 0.4 }
  },
  {
    id: "full-range",
    name: "Full Range",
    description: "OC44 #3 with 47 nF cap, 85% Boost, 0.35 V/FS for extended bass.",
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: "oc44-03", rangeNf: 47, boost: 0.85 }
  },
  {
    id: "low-input-clean",
    name: "Low Input / Clean Boost",
    description: "0.12 V/FS and 70% Boost for low-THD dynamic treble lift.",
    config: { ...NOMINAL_DEFAULT_CONFIG, inputVolts: 0.12, boost: 0.7 }
  },
  {
    id: "high-input-drive",
    name: "High Input / Germanium Drive",
    description: "0.90 V/FS driving the PNP stage deeply into asymmetric germanium clipping.",
    config: { ...NOMINAL_DEFAULT_CONFIG, inputVolts: 0.9, boost: 1, outputTrimDb: -6 }
  },
  {
    id: "ac128-alternative",
    name: "AC128 Alternative",
    description: "AC128 #1* profile (higher leakage, 100 pF Ccb, missing Ceb in datasheet).",
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: "ac128-01", inputVolts: 0.4 }
  },
  {
    id: "holmes-calibrated-exp",
    name: "Holmes Calibrated \u2014 Experimental",
    description: "Engine E (Holmes 2019 Table 6.2) isolated literature parameter set.",
    config: { ...NOMINAL_DEFAULT_CONFIG, engine: "calibrated-oc44" }
  }
];

// src/worklet/rangemaster-processor.ts
var MAX_QUANTUM = 1024;
var RING_SIZE = 2048;
var RING_MASK = RING_SIZE - 1;
var SCOPE_LEN = 512;
var WARMUP_SAMPLES = 256;
var XFADE_SAMPLES = 256;
var BYPASS_XFADE_SAMPLES = 64;
function nowMicros() {
  if (typeof performance !== "undefined" && typeof performance.now === "function") {
    return performance.now() * 1e3;
  }
  return Date.now() * 1e3;
}
function cloneConfig(c) {
  return { ...c, gpAblation: { ...c.gpAblation } };
}
function isStructuralChange(a, b) {
  return a.engine !== b.engine || a.transistorId !== b.transistorId || a.oversampling !== b.oversampling || a.rangeNf !== b.rangeNf || ablationToMask(a.gpAblation) !== ablationToMask(b.gpAblation);
}
function isCategoryAOnly(a, b) {
  return !isStructuralChange(a, b) && a.boost === b.boost && a.potTaper === b.potTaper && a.boostLinearApprox === b.boostLinearApprox && a.biasTrim === b.biasTrim && a.supplyVolts === b.supplyVolts && a.sourceOhms === b.sourceOhms && a.loadOhms === b.loadOhms;
}
var RangemasterWorkletProcessor = class extends AudioWorkletProcessor {
  M = null;
  ready = false;
  hActive = 0;
  hCand = 0;
  inPtr = 0;
  outActivePtr = 0;
  outCandPtr = 0;
  diagPtr = 0;
  dcPtr = 0;
  condPtr = 0;
  inView = null;
  outActiveView = null;
  outCandView = null;
  activeConfig = cloneConfig(NOMINAL_DEFAULT_CONFIG);
  candConfig = cloneConfig(NOMINAL_DEFAULT_CONFIG);
  pendingConfig = null;
  activeSlot = "A";
  candSlot = "A";
  bypassed = false;
  bypassMix = 0;
  // 0 = wet, 1 = dry
  targetLevelMatchGain = 1;
  currentLevelMatchGain = 1;
  candLevelMatchGain = 1;
  activeLatency = 0;
  candLatency = 0;
  // Fractional latency alignment rings (pre-allocated, zero-GC)
  dryRing = new Float32Array(RING_SIZE);
  dryWrite = 0;
  activeDelayRing = new Float32Array(RING_SIZE);
  activeDelayWrite = 0;
  candDelayRing = new Float32Array(RING_SIZE);
  candDelayWrite = 0;
  warmupInputRing = new Float32Array(RING_SIZE);
  warmupWrite = 0;
  // Crossfade state for Category C & A/B switches
  xfadeActive = false;
  xfadePos = 0;
  // Scope ring (pre-allocated)
  scopeInRing = new Float32Array(SCOPE_LEN);
  scopeOutRing = new Float32Array(SCOPE_LEN);
  scopeWrite = 0;
  scopeInSnap = new Array(SCOPE_LEN).fill(0);
  scopeOutSnap = new Array(SCOPE_LEN).fill(0);
  // Telemetry accumulators
  samplesSinceTelemetry = 0;
  telemetryIntervalSamples = 4096;
  blockPeakIn = 0;
  blockSqIn = 0;
  blockPeakOut = 0;
  blockSqOut = 0;
  blockCountStats = 0;
  procUsSum = 0;
  procUsMax = 0;
  deadlineOverruns = 0;
  lastError = null;
  constructor() {
    super();
    this.telemetryIntervalSamples = Math.max(2048, Math.round(sampleRate * 0.08));
    this.port.onmessage = (ev) => {
      this.handleMessage(ev.data);
    };
  }
  handleMessage(msg) {
    const type = msg?.type;
    if (type === "init-wasm") {
      const wasmBinary = msg.wasmBinary;
      const initialConfig = msg.config || NOMINAL_DEFAULT_CONFIG;
      const initialSlot = msg.slot || "A";
      void this.initWasm(wasmBinary, initialConfig, initialSlot);
      return;
    }
    if (!this.ready || !this.M) {
      return;
    }
    if (type === "set-config") {
      const cfg = msg.config;
      const slot = msg.slot || this.activeSlot;
      const matchGain = typeof msg.levelMatchGain === "number" && Number.isFinite(msg.levelMatchGain) ? msg.levelMatchGain : 1;
      this.applyConfigUpdate(cfg, slot, matchGain);
    } else if (type === "set-bypass") {
      this.bypassed = Boolean(msg.bypassed);
    } else if (type === "set-level-match") {
      const g = Number(msg.gain);
      if (Number.isFinite(g) && g >= 0.01 && g <= 100) {
        this.targetLevelMatchGain = g;
      }
    } else if (type === "reset-diagnostics") {
      this.M._rangemaster_reset_diagnostics(this.hActive);
      this.procUsSum = 0;
      this.procUsMax = 0;
      this.blockCountStats = 0;
      this.deadlineOverruns = 0;
    } else if (type === "reset-circuit") {
      this.M._rangemaster_reset(this.hActive);
    }
  }
  async initWasm(wasmBinary, initialConfig, slot) {
    try {
      const M = await instantiateRangemasterWasm(wasmBinary);
      const hA = M._rangemaster_create();
      const hB = M._rangemaster_create();
      if (!hA || !hB) {
        throw new Error("Failed to create WASM RangemasterHandle");
      }
      M._rangemaster_set_solver_profile(hA, 0);
      M._rangemaster_set_solver_profile(hB, 0);
      const resA = applyFullConfigToHandle(M, hA, initialConfig);
      const resB = applyFullConfigToHandle(M, hB, initialConfig);
      if (!resA.ok || !resB.ok) {
        throw new Error(resA.error || resB.error || "Initial config rejected");
      }
      if (M._rangemaster_prepare(hA, sampleRate, MAX_QUANTUM) !== 1 || M._rangemaster_prepare(hB, sampleRate, MAX_QUANTUM) !== 1) {
        throw new Error("Failed to prepare WASM RangemasterEngine");
      }
      const inPtr = M._malloc(MAX_QUANTUM * 8);
      const outActivePtr = M._malloc(MAX_QUANTUM * 8);
      const outCandPtr = M._malloc(MAX_QUANTUM * 8);
      const diagPtr = M._malloc(256);
      const dcPtr = M._malloc(128);
      const condPtr = M._malloc(32);
      this.M = M;
      this.hActive = hA;
      this.hCand = hB;
      this.inPtr = inPtr;
      this.outActivePtr = outActivePtr;
      this.outCandPtr = outCandPtr;
      this.diagPtr = diagPtr;
      this.dcPtr = dcPtr;
      this.condPtr = condPtr;
      this.inView = new Float64Array(M.HEAPF64.buffer, inPtr, MAX_QUANTUM);
      this.outActiveView = new Float64Array(M.HEAPF64.buffer, outActivePtr, MAX_QUANTUM);
      this.outCandView = new Float64Array(M.HEAPF64.buffer, outCandPtr, MAX_QUANTUM);
      this.activeConfig = cloneConfig(initialConfig);
      this.candConfig = cloneConfig(initialConfig);
      this.activeSlot = slot;
      this.activeLatency = M._rangemaster_get_latency_samples(hA);
      this.candLatency = this.activeLatency;
      this.ready = true;
      this.lastError = null;
      this.port.postMessage({
        type: "wasm-ready",
        sampleRate,
        latencySamples: this.activeLatency
      });
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      this.lastError = msg;
      this.port.postMessage({ type: "wasm-error", error: msg });
    }
  }
  applyConfigUpdate(cfg, slot, matchGain) {
    if (!this.M || !this.ready) return;
    if (this.xfadeActive) {
      this.pendingConfig = { config: cloneConfig(cfg), slot };
      this.targetLevelMatchGain = matchGain;
      return;
    }
    if (slot === this.activeSlot && !isStructuralChange(this.activeConfig, cfg)) {
      if (isCategoryAOnly(this.activeConfig, cfg)) {
        const okIn = this.M._rangemaster_set_input_volts(this.hActive, cfg.inputVolts);
        const okRef = this.M._rangemaster_set_output_ref_volts(this.hActive, cfg.outputRefVolts);
        const okTrim = this.M._rangemaster_set_output_trim_db(this.hActive, cfg.outputTrimDb);
        if (okIn !== 1 || okRef !== 1 || okTrim !== 1) {
          this.rejectUpdate("Digital gain parameter out of bounds");
          return;
        }
        this.activeConfig.inputVolts = cfg.inputVolts;
        this.activeConfig.outputRefVolts = cfg.outputRefVolts;
        this.activeConfig.outputTrimDb = cfg.outputTrimDb;
        this.targetLevelMatchGain = matchGain;
        return;
      }
      const res2 = applyFullConfigToHandle(this.M, this.hActive, cfg);
      if (!res2.ok) {
        this.rejectUpdate(res2.error || "Circuit update rejected");
        return;
      }
      this.activeConfig = cloneConfig(cfg);
      this.targetLevelMatchGain = matchGain;
      return;
    }
    const res = applyFullConfigToHandle(this.M, this.hCand, cfg);
    if (!res.ok) {
      this.rejectUpdate(res.error || "Structural candidate rejected");
      return;
    }
    if (this.M._rangemaster_reset(this.hCand) !== 1) {
      this.rejectUpdate("Candidate reset failed");
      return;
    }
    if (this.inView) {
      const start = this.warmupWrite - WARMUP_SAMPLES + RING_SIZE & RING_MASK;
      for (let i = 0; i < WARMUP_SAMPLES; ++i) {
        this.inView[i] = this.warmupInputRing[start + i & RING_MASK];
      }
      this.M._rangemaster_process_f64(this.hCand, this.inPtr, this.outCandPtr, WARMUP_SAMPLES);
    }
    this.candConfig = cloneConfig(cfg);
    this.candSlot = slot;
    this.candLatency = this.M._rangemaster_get_latency_samples(this.hCand);
    this.candLevelMatchGain = matchGain;
    this.activeDelayRing.fill(0);
    this.candDelayRing.fill(0);
    this.xfadePos = 0;
    this.xfadeActive = true;
  }
  rejectUpdate(reason) {
    this.lastError = reason;
    this.port.postMessage({
      type: "param-rejected",
      reason,
      activeConfig: cloneConfig(this.activeConfig),
      activeSlot: this.activeSlot
    });
  }
  readDelayed(ring, writeIdx, delaySamples) {
    if (delaySamples <= 0) {
      return ring[writeIdx - 1 + RING_SIZE & RING_MASK];
    }
    const clamped = Math.min(delaySamples, RING_SIZE - 4);
    const dInt = Math.floor(clamped);
    const frac = clamped - dInt;
    const i0 = writeIdx - 1 - dInt + RING_SIZE & RING_MASK;
    const i1 = writeIdx - 2 - dInt + RING_SIZE & RING_MASK;
    return ring[i0] * (1 - frac) + ring[i1] * frac;
  }
  process(inputs, outputs) {
    const out = outputs[0];
    if (!out || out.length === 0) return true;
    const outL = out[0];
    const n = outL.length;
    const inGroup = inputs[0];
    const inL = inGroup && inGroup.length > 0 ? inGroup[0] : null;
    const inR = inGroup && inGroup.length > 1 ? inGroup[1] : null;
    if (!this.ready || !this.M || !this.inView || !this.outActiveView || !this.outCandView || n > MAX_QUANTUM) {
      for (let i = 0; i < n; ++i) {
        const s = inL ? inL[i] : 0;
        outL[i] = Number.isFinite(s) ? s : 0;
      }
      for (let ch = 1; ch < out.length; ++ch) {
        out[ch].set(outL);
      }
      return true;
    }
    const t0 = nowMicros();
    const M = this.M;
    for (let i = 0; i < n; ++i) {
      let s = 0;
      if (inL && inR) {
        s = 0.5 * (inL[i] + inR[i]);
      } else if (inL) {
        s = inL[i];
      }
      if (!Number.isFinite(s)) {
        s = 0;
      }
      this.inView[i] = s;
      this.warmupInputRing[this.warmupWrite] = s;
      this.warmupWrite = this.warmupWrite + 1 & RING_MASK;
      this.dryRing[this.dryWrite] = s;
      this.dryWrite = this.dryWrite + 1 & RING_MASK;
      const absS = Math.abs(s);
      if (absS > this.blockPeakIn) this.blockPeakIn = absS;
      this.blockSqIn += s * s;
    }
    M._rangemaster_process_f64(this.hActive, this.inPtr, this.outActivePtr, n);
    if (this.xfadeActive) {
      M._rangemaster_process_f64(this.hCand, this.inPtr, this.outCandPtr, n);
    }
    const targetBypass = this.bypassed ? 1 : 0;
    const bypassStep = 1 / BYPASS_XFADE_SAMPLES;
    const maxLat = this.xfadeActive ? Math.max(this.activeLatency, this.candLatency) : this.activeLatency;
    const extraActiveDelay = Math.max(0, maxLat - this.activeLatency);
    const extraCandDelay = Math.max(0, maxLat - this.candLatency);
    for (let i = 0; i < n; ++i) {
      const wetActRaw = this.outActiveView[i];
      this.activeDelayRing[this.activeDelayWrite] = wetActRaw;
      this.activeDelayWrite = this.activeDelayWrite + 1 & RING_MASK;
      let wetSample = wetActRaw;
      if (this.xfadeActive) {
        const wetCandRaw = this.outCandView[i];
        this.candDelayRing[this.candDelayWrite] = wetCandRaw;
        this.candDelayWrite = this.candDelayWrite + 1 & RING_MASK;
        const aAligned = this.readDelayed(this.activeDelayRing, this.activeDelayWrite, extraActiveDelay) * this.currentLevelMatchGain;
        const cAligned = this.readDelayed(this.candDelayRing, this.candDelayWrite, extraCandDelay) * this.candLevelMatchGain;
        const t = Math.min(1, this.xfadePos / XFADE_SAMPLES);
        const wCand = 0.5 * (1 - Math.cos(Math.PI * t));
        wetSample = (1 - wCand) * aAligned + wCand * cAligned;
        ++this.xfadePos;
        if (this.xfadePos >= XFADE_SAMPLES) {
          this.finishCrossfade();
        }
      } else {
        this.currentLevelMatchGain += 0.01 * (this.targetLevelMatchGain - this.currentLevelMatchGain);
        wetSample = wetActRaw * this.currentLevelMatchGain;
      }
      if (this.bypassMix < targetBypass) {
        this.bypassMix = Math.min(targetBypass, this.bypassMix + bypassStep);
      } else if (this.bypassMix > targetBypass) {
        this.bypassMix = Math.max(targetBypass, this.bypassMix - bypassStep);
      }
      const dryReadBase = this.dryWrite - (n - 1 - i) + RING_SIZE * 2 & RING_MASK;
      const dryAligned = this.readDelayed(this.dryRing, dryReadBase, this.activeLatency);
      const outSample = (1 - this.bypassMix) * wetSample + this.bypassMix * dryAligned;
      outL[i] = Number.isFinite(outSample) ? outSample : 0;
      const absOut = Math.abs(wetSample);
      if (absOut > this.blockPeakOut) this.blockPeakOut = absOut;
      this.blockSqOut += wetSample * wetSample;
      this.scopeInRing[this.scopeWrite] = this.inView[i];
      this.scopeOutRing[this.scopeWrite] = wetSample;
      this.scopeWrite = this.scopeWrite + 1 & SCOPE_LEN - 1;
    }
    for (let ch = 1; ch < out.length; ++ch) {
      out[ch].set(outL);
    }
    const elapsedUs = Math.max(0, nowMicros() - t0);
    const quantumBudgetUs = n / sampleRate * 1e6;
    this.procUsSum += elapsedUs;
    if (elapsedUs > this.procUsMax) this.procUsMax = elapsedUs;
    ++this.blockCountStats;
    if (elapsedUs > quantumBudgetUs) {
      ++this.deadlineOverruns;
    }
    this.samplesSinceTelemetry += n;
    if (this.samplesSinceTelemetry >= this.telemetryIntervalSamples) {
      this.emitTelemetry(quantumBudgetUs);
    }
    return true;
  }
  finishCrossfade() {
    const tmpH = this.hActive;
    this.hActive = this.hCand;
    this.hCand = tmpH;
    this.activeConfig = cloneConfig(this.candConfig);
    this.activeSlot = this.candSlot;
    this.activeLatency = this.candLatency;
    this.targetLevelMatchGain = this.candLevelMatchGain;
    this.currentLevelMatchGain = this.candLevelMatchGain;
    this.xfadeActive = false;
    this.xfadePos = 0;
    if (this.pendingConfig) {
      const pend = this.pendingConfig;
      this.pendingConfig = null;
      this.applyConfigUpdate(pend.config, pend.slot, this.targetLevelMatchGain);
    }
  }
  emitTelemetry(quantumBudgetUs) {
    if (!this.M) return;
    const M = this.M;
    M._rangemaster_get_diagnostics(this.hActive, this.diagPtr);
    M._rangemaster_get_dc_operating_point(this.hActive, this.dcPtr);
    const diag = readDiagnosticsFromPtr(M, this.diagPtr);
    const dcOp = readDcOpFromPtr(M, this.dcPtr);
    const cond = readConditioning(M, this.hActive, this.condPtr);
    const nSamples = Math.max(1, this.samplesSinceTelemetry);
    for (let i = 0; i < SCOPE_LEN; ++i) {
      const idx = this.scopeWrite + i & SCOPE_LEN - 1;
      this.scopeInSnap[i] = this.scopeInRing[idx];
      this.scopeOutSnap[i] = this.scopeOutRing[idx];
    }
    this.port.postMessage({
      type: "telemetry",
      telemetry: {
        sampleRate,
        internalSampleRate: M._rangemaster_get_internal_sample_rate(this.hActive),
        latencySamples: this.activeLatency,
        matrixRebuildCount: M._rangemaster_get_matrix_rebuild_count(this.hActive),
        activeSlot: this.activeSlot,
        bypassed: this.bypassed,
        switching: this.xfadeActive,
        inPeak: this.blockPeakIn,
        inRms: Math.sqrt(this.blockSqIn / nSamples),
        outPeak: this.blockPeakOut,
        outRms: Math.sqrt(this.blockSqOut / nSamples),
        clipped: this.blockPeakOut > 1,
        processTimeMeanUs: this.blockCountStats > 0 ? this.procUsSum / this.blockCountStats : 0,
        processTimeMaxUs: this.procUsMax,
        quantumBudgetUs,
        deadlineOverruns: this.deadlineOverruns,
        dcOp,
        diag,
        cond,
        scopeInput: this.scopeInSnap,
        scopeOutput: this.scopeOutSnap,
        lastError: this.lastError
      }
    });
    this.samplesSinceTelemetry = 0;
    this.blockPeakIn = 0;
    this.blockSqIn = 0;
    this.blockPeakOut = 0;
    this.blockSqOut = 0;
    this.procUsSum = 0;
    this.procUsMax = 0;
    this.blockCountStats = 0;
  }
};
registerProcessor("rangemaster-processor", RangemasterWorkletProcessor);
