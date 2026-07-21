const path = require("path");

const projectRoot = path.resolve(__dirname, "..");
const ccxml = path.join(projectRoot, "targetConfigs", "MSPM0G3507.ccxml");
const program = path.join(projectRoot, "Debug", "cy_template.out");

const ds = initScripting();

try {
    ds.setScriptingTimeout(120000);
    console.log(`CCXML=${ccxml}`);
    console.log(`PROGRAM=${program}`);

    ds.configure(ccxml);
    const session = ds.openSession(/cortex/i);

    console.log("Connecting target...");
    session.target.connect();

    console.log("Loading program...");
    session.memory.loadProgram(program);

    console.log("Reset target...");
    session.target.reset();

    console.log("Run target...");
    ds.setScriptingTimeout(2000);
    try {
        session.target.run();
        console.log("Target halted after run");
    } catch (runErr) {
        if (runErr instanceof ScriptingTimeoutError) {
            console.log("Target is running");
        } else {
            throw runErr;
        }
    }

    console.log("Flash OK");
} catch (err) {
    console.error("Flash FAILED");
    console.error(err && err.stack ? err.stack : err);
    process.exitCode = 1;
} finally {
    try {
        ds.shutdown();
    } catch (shutdownErr) {
        console.error("Debug server shutdown FAILED");
        console.error(shutdownErr && shutdownErr.stack ? shutdownErr.stack : shutdownErr);
        process.exitCode = 1;
    }
}
