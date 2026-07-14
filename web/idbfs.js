Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  const saveDirectory = '/joji-save';
  try { FS.mkdir(saveDirectory); } catch (error) {}
  FS.mount(IDBFS, {}, saveDirectory);
  addRunDependency('joji-save-load');
  FS.syncfs(true, function (error) {
    if (error) console.error('JOJIFrontier save load failed', error);
    removeRunDependency('joji-save-load');
  });
});
