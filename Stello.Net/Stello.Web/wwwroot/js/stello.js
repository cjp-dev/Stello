// Browser helpers called from .NET through JS interop.
window.stello = (() => {
    const maxFileSize = 1_000_000;
    const shortcutKeys = 'osmztyfa';
    let keydown = null;
    let audio = null;

    return {
        // Ctrl+key shortcuts from the desktop version; Ctrl+N belongs to the browser.
        registerShortcuts(dotnet) {
            keydown = e => {
                if (!(e.ctrlKey || e.metaKey) || e.altKey || e.shiftKey || e.key.length !== 1) {
                    return;
                }
                const key = e.key.toLowerCase();
                if (!shortcutKeys.includes(key) || e.target instanceof HTMLInputElement) {
                    return;
                }
                e.preventDefault();
                dotnet.invokeMethodAsync('OnShortcut', key);
            };
            document.addEventListener('keydown', keydown);
        },

        unregisterShortcuts() {
            document.removeEventListener('keydown', keydown);
        },

        // Resolves to [name, text], or null if the user cancels.
        openTextFile(accept) {
            return new Promise((resolve, reject) => {
                const input = document.createElement('input');
                input.type = 'file';
                input.accept = accept;
                input.addEventListener('cancel', () => resolve(null));
                input.addEventListener('change', async () => {
                    const file = input.files[0];
                    if (!file) {
                        resolve(null);
                    } else if (file.size > maxFileSize) {
                        reject(new Error('The file is too large to be a game.'));
                    } else {
                        resolve([file.name, await file.text()]);
                    }
                });
                input.click();
            });
        },

        downloadTextFile(name, text) {
            const url = URL.createObjectURL(new Blob([text], { type: 'text/plain' }));
            const link = document.createElement('a');
            link.href = url;
            link.download = name;
            document.body.appendChild(link);
            link.click();
            link.remove();
            // Some browsers still read the URL after click() returns.
            setTimeout(() => URL.revokeObjectURL(url), 10_000);
        },

        beep() {
            audio ??= new AudioContext();
            const oscillator = audio.createOscillator();
            const gain = audio.createGain();
            oscillator.frequency.value = 880;
            gain.gain.value = 0.1;
            oscillator.connect(gain).connect(audio.destination);
            oscillator.start();
            oscillator.stop(audio.currentTime + 0.1);
        },
    };
})();
