module.exports =  function() {
  try {
    return require('./build/Release/addon.node');
  } catch (err) {
    return require('./build/Debug/addon.node');
  }
}
