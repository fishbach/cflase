(() => {

const laserURL = new URL(document.currentScript.src).origin;
Promise.all([
    import(laserURL + '/js/cflib/net/rmi.mjs'),
    import(laserURL + '/js/services/laserservice.mjs'),
    import(laserURL + '/js/dao/laserpoint.mjs')
]).then(mods => {
    const rmi    = mods[0].default;
    window.laser = mods[1].default;
    laser.Point  = mods[2].default;

    laser.errorCallback     = null;
    laser.activeCallback    = null;
    laser.finishedCallback  = null;
    laser.MaxSpeed          = 59899;
    laser.OptimalPointCount = 8190;

    rmi.start(laserURL + '/ws');
    laser.idle();
    laser.rsig.error.bind((error) => {
        laser.errorCallback && laser.errorCallback(error);
    }).register();
    laser.rsig.active.bind((active) => {
        laser.activeCallback && laser.activeCallback(active);
    }).register();
    laser.rsig.finished.bind(() => {
        laser.finishedCallback && laser.finishedCallback();
    }).register();
    initLaser();
});

})();