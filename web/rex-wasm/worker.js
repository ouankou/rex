self.onmessage = async (event) => {
  if (!event.data || event.data.type !== "run") {
    return;
  }

  try {
    importScripts("rex_wasm.js");
    self.postMessage({type : "ready"});

    const module = await createRexModule({
      locateFile : (file) => file,
      print : () => {},
      printErr : () => {},
    });

    const rawResult = module.runRex(
        event.data.source,
        event.data.filename,
        event.data.mode,
    );
    self.postMessage({type : "result", result : JSON.parse(rawResult)});
  } catch (error) {
    self.postMessage({
      type : "error",
      message : error && error.stack ? error.stack : String(error),
    });
  }
};
