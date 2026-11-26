Module.preRun = [function () {
  FS.mkdir('/workspace');
  FS.mount(NODEFS, { root: process.cwd() }, '/workspace');
  FS.chdir('/workspace');
}];
