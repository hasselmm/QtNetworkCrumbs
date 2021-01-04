const core = require('@actions/core');

function run() {
    try {
        const qtdir = core.getInput("qtdir") || (process.env.RUNNER_WORKSPACE + "/Qt");
        const tools = core.getInput("tools");

        if (tools) {
            tools.split(" ").forEach(element => {
                const elements = element.split(",");

                if (elements[0] === "tools_ninja") {
                     const ninjadir = qtdir + "/Tools/Ninja";
                     core.info("Enabling Ninja in " + ninjadir)
                     core.exportVariable("NINJA_PATH", ninjadir);
                     core.addPath(ninjadir);
                } else if (elements[0] === "tools_mingw") {
                    const variant = elements[2].replace(/^qt\.tools\.[^_]+_/, "");
                    const mingwdir = qtdir + "/Tools/" + variant;
                    core.info("Enabling " + variant + " in " + mingwdir)
                    core.exportVariable(variant.toUpperCase() + "_PATH", mingwdir);
                    core.addPath(mingwdir + "/bin");
                } else {
                    core.warning("Ignoring unknown tool " + elements[0]);
                }
            });
        } else {
            core.info("No tools to enable");
        }
    } catch (error) {
      core.setFailed(error.message);
    }
}

run();
