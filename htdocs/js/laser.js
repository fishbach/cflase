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
    laser.OptimalPointCount = 8190 * 2;
    laser.PPS               = laser.MaxSpeed;

    const idleOrig = laser.idle;
    laser.idle = (retFunc) => {
        laser.finishedCallback = null;
        idleOrig.call(laser, retFunc);
    };

    laser.showFunc = (gen) => {
        const it = gen();
        const points = () => {
            let p = [];
            while (p.length < laser.OptimalPointCount) p.push(it.next().value);
            return p;
        };
        laser.finishedCallback = () => { laser.show(points(), false, laser.MaxSpeed); };
        laser.show(points(), false, laser.MaxSpeed);
    };

    rmi.start(laserURL + '/ws');
    laser.idle(() => {
        laser.rsig.error   .bind((error ) => { laser.errorCallback    && laser.errorCallback   (error ); }).register();
        laser.rsig.active  .bind((active) => { laser.activeCallback   && laser.activeCallback  (active); }).register();
        laser.rsig.finished.bind((      ) => { laser.finishedCallback && laser.finishedCallback(      ); }).register();
        laser.identity();
        initLaser();
    });
});

})();