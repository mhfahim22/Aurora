const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const cp = require('child_process');

/** Check if JetBrains Mono is installed via registry */
function isFontInstalled() {
    try {
        const regPath = 'HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Fonts';
        const result = cp.execSync(
            `reg query "${regPath}" /f "JetBrains Mono" 2>nul`,
            { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }
        );
        return result.includes('JetBrains Mono');
    } catch {
        return false;
    }
}

/** Run the font installer PowerShell script */
function installFont() {
    const scriptPath = path.join(__dirname, 'scripts', 'install-font.ps1');
    if (!fs.existsSync(scriptPath)) {
        vscode.window.showErrorMessage(
            'Font installer script not found. Install JetBrains Mono manually from https://www.jetbrains.com/lp/mono/'
        );
        return;
    }

    vscode.window.withProgress(
        {
            location: vscode.ProgressLocation.Notification,
            title: 'Installing JetBrains Mono...',
            cancellable: false,
        },
        async () => {
            return new Promise((resolve) => {
                const child = cp.exec(
                    `powershell -NoProfile -ExecutionPolicy Bypass -File "${scriptPath}" -Silent`,
                    { windowsHide: true },
                    (error, stdout, stderr) => {
                        if (error) {
                            vscode.window.showErrorMessage(
                                'Font install failed. Download manually: https://www.jetbrains.com/lp/mono/'
                            );
                        } else {
                            vscode.window.showInformationMessage(
                                '✓ JetBrains Mono installed! Restart VS Code to apply.',
                                'Restart Now'
                            ).then((selection) => {
                                if (selection === 'Restart Now') {
                                    vscode.commands.executeCommand('workbench.action.reloadWindow');
                                }
                            });
                        }
                        resolve();
                    }
                );
            });
        }
    );
}

function activate(context) {
    // Only prompt on first install (use globalState)
    const key = 'aurora.fontPromptShown';
    if (!context.globalState.get(key)) {
        // Small delay so VS Code finishes loading
        setTimeout(() => {
            if (!isFontInstalled()) {
                vscode.window.showInformationMessage(
                    'Aurora Synthwave theme looks best with JetBrains Mono. Install it now?',
                    'Install Font',
                    'Not Now'
                ).then((selection) => {
                    if (selection === 'Install Font') {
                        installFont();
                    }
                    context.globalState.update(key, true);
                });
            } else {
                context.globalState.update(key, true);
            }
        }, 2000);
    }

    // Also expose a command to manually trigger font install
    context.subscriptions.push(
        vscode.commands.registerCommand('aurora.installFont', installFont)
    );
}

function deactivate() {}

module.exports = { activate, deactivate };
