const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const cp = require('child_process');

/**
 * Check if JetBrains Mono is installed via registry
 */
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

/**
 * Run the font installer PowerShell script
 */
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

/**
 * Find the aurora_lsp binary
 */
function findLspBinary() {
    const possiblePaths = [
        path.join(__dirname, '..', '..', 'build', 'Release', 'aurora_lsp.exe'),
        path.join(__dirname, '..', '..', 'build', 'Debug', 'aurora_lsp.exe'),
        path.join(__dirname, '..', '..', 'build', 'aurora_lsp.exe'),
        'aurora_lsp.exe',
        'aurora_lsp',
    ];

    for (const p of possiblePaths) {
        if (fs.existsSync(p)) return p;
    }

    // Try to find in PATH
    try {
        const which = cp.execSync('where aurora_lsp 2>nul || which aurora_lsp 2>/dev/null',
            { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
        const lines = which.trim().split('\n');
        if (lines.length > 0 && lines[0].length > 0) return lines[0].trim();
    } catch { }

    return null;
}

/**
 * LSP client — manages the aurora_lsp server process
 */
class AuroraLspClient {
    constructor() {
        this.serverProcess = null;
        this.outputChannel = null;
        this.pendingRequests = new Map();
        this.requestId = 1;
        this.buffer = '';
        this.ready = false;
        this.outputEditors = new Set();
    }

    async start() {
        const lspPath = findLspBinary();
        this.outputChannel = vscode.window.createOutputChannel('Aurora LSP');

        if (!lspPath) {
            this.outputChannel.appendLine(
                'aurora_lsp not found. Build with: cmake --build build --target aurora_lsp\n' +
                'Language features (autocomplete, diagnostics, etc) disabled until LSP is available.\n' +
                'To hide this, set "aurora.lsp.enabled": false in settings.'
            );
            return false;
        }
        this.outputChannel.appendLine(`Starting aurora_lsp from: ${lspPath}`);

        try {
            this.serverProcess = cp.spawn(lspPath, [], {
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true,
            });

            this.serverProcess.stdout.on('data', (data) => {
                this.handleServerMessage(data.toString());
            });

            this.serverProcess.stderr.on('data', (data) => {
                this.outputChannel.appendLine(`[stderr] ${data.toString()}`);
            });

            this.serverProcess.on('error', (err) => {
                this.outputChannel.appendLine(`[error] ${err.message}`);
                this.ready = false;
            });

            this.serverProcess.on('exit', (code) => {
                this.outputChannel.appendLine(`Server exited with code ${code}`);
                this.ready = false;
            });

            // Send initialize request
            const initParams = {
                processId: process.pid,
                capabilities: {
                    textDocument: {
                        completion: { completionItem: { snippetSupport: true } },
                        semanticTokens: {
                            requests: { full: true, range: false },
                            tokenTypes: [
                                'variable', 'function', 'method', 'class', 'interface',
                                'enum', 'struct', 'parameter', 'property', 'keyword',
                                'comment', 'string', 'number', 'operator', 'type',
                                'namespace', 'macro', 'modifier', 'event', 'decorator'
                            ],
                            tokenModifiers: [
                                'declaration', 'definition', 'readonly', 'static',
                                'abstract', 'deprecated', 'async', 'modification', 'documentation'
                            ]
                        },
                    },
                    workspace: {
                        didChangeConfiguration: {},
                    },
                },
                rootUri: vscode.workspace.workspaceFolders
                    ? vscode.workspace.workspaceFolders[0].uri.toString()
                    : null,
            };

            await this.sendRequest('initialize', initParams);
            await this.sendNotification('initialized', {});
            this.ready = true;
            this.outputChannel.appendLine('LSP server initialized successfully');
            return true;
        } catch (err) {
            this.outputChannel.appendLine(`Failed to start LSP: ${err.message}`);
            return false;
        }
    }

    handleServerMessage(data) {
        this.buffer += data;
        while (true) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd === -1) break;

            const header = this.buffer.substring(0, headerEnd);
            const contentMatch = header.match(/Content-Length:\s*(\d+)/i);
            if (!contentMatch) { this.buffer = ''; break; }

            const contentLength = parseInt(contentMatch[1], 10);
            const bodyStart = headerEnd + 4;

            if (this.buffer.length < bodyStart + contentLength) break;

            const body = this.buffer.substring(bodyStart, bodyStart + contentLength);
            this.buffer = this.buffer.substring(bodyStart + contentLength);

            try {
                const msg = JSON.parse(body);
                this.handleMessage(msg);
            } catch (e) {
                this.outputChannel.appendLine(`Failed to parse LSP message: ${e.message}`);
            }
        }
    }

    handleMessage(msg) {
        if (msg.id !== undefined && this.pendingRequests.has(msg.id)) {
            const { resolve } = this.pendingRequests.get(msg.id);
            this.pendingRequests.delete(msg.id);
            resolve(msg);
        } else if (msg.method === 'textDocument/publishDiagnostics') {
            const diagnostics = msg.params;
            if (diagnostics && diagnostics.uri) {
                const uri = vscode.Uri.parse(diagnostics.uri);
                const diag = (diagnostics.diagnostics || []).map(d => {
                    const severity = d.severity === 1 ? vscode.DiagnosticSeverity.Error
                        : d.severity === 2 ? vscode.DiagnosticSeverity.Warning
                            : d.severity === 3 ? vscode.DiagnosticSeverity.Information
                                : vscode.DiagnosticSeverity.Hint;
                    return new vscode.Diagnostic(
                        new vscode.Range(
                            d.range.start.line, d.range.start.character,
                            d.range.end.line, d.range.end.character
                        ),
                        d.message,
                        severity
                    );
                });
                const diagnosticCollection = AuroraLspClient.diagCollection;
                if (diagnosticCollection) {
                    diagnosticCollection.set(uri, diag);
                }
            }
        }
    }

    sendNotification(method, params) {
        if (!this.serverProcess || !this.serverProcess.stdin.writable) return;
        const body = JSON.stringify({ jsonrpc: '2.0', method, params });
        const header = `Content-Length: ${Buffer.byteLength(body, 'utf-8')}\r\n\r\n`;
        this.serverProcess.stdin.write(header + body);
    }

    sendRequest(method, params) {
        return new Promise((resolve, reject) => {
            if (!this.serverProcess || !this.serverProcess.stdin.writable) {
                reject(new Error('Server not running'));
                return;
            }
            const id = this.requestId++;
            const body = JSON.stringify({ jsonrpc: '2.0', id, method, params });
            const header = `Content-Length: ${Buffer.byteLength(body, 'utf-8')}\r\n\r\n`;
            this.serverProcess.stdin.write(header + body);
            this.pendingRequests.set(id.toString(), { resolve, reject });
            setTimeout(() => {
                if (this.pendingRequests.has(id.toString())) {
                    this.pendingRequests.delete(id.toString());
                    reject(new Error('Request timeout'));
                }
            }, 10000);
        });
    }

    sendNotificationToServer(method, params) {
        this.sendNotification(method, params);
    }

    async stop() {
        if (this.ready) {
            try {
                await this.sendRequest('shutdown', {});
            } catch { }
            this.sendNotification('exit', {});
        }
        if (this.serverProcess) {
            this.serverProcess.kill();
            this.serverProcess = null;
        }
        this.ready = false;
    }
}

// Static diagnostic collection
AuroraLspClient.diagCollection = null;

/**
 * Extension activation
 */
let lspClient = null;

function activate(context) {
    // Create diagnostic collection
    const diagCollection = vscode.languages.createDiagnosticCollection('aurora');
    AuroraLspClient.diagCollection = diagCollection;
    context.subscriptions.push(diagCollection);

    // Prompt for font install
    const key = 'aurora.fontPromptShown';
    if (!context.globalState.get(key)) {
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

    // Register font install command
    context.subscriptions.push(
        vscode.commands.registerCommand('aurora.installFont', installFont)
    );

    // Start LSP client
    lspClient = new AuroraLspClient();
    lspClient.start().then((started) => {
        if (started) {
            const outputChannel = lspClient.outputChannel;

            // Track open/close documents
            context.subscriptions.push(
                vscode.workspace.onDidOpenTextDocument((doc) => {
                    if (doc.languageId === 'aurora') {
                        lspClient.sendNotificationToServer('textDocument/didOpen', {
                            textDocument: {
                                uri: doc.uri.toString(),
                                languageId: 'aurora',
                                version: 1,
                                text: doc.getText(),
                            }
                        });
                    }
                }),
                vscode.workspace.onDidChangeTextDocument((e) => {
                    if (e.document.languageId === 'aurora') {
                        lspClient.sendNotificationToServer('textDocument/didChange', {
                            textDocument: {
                                uri: e.document.uri.toString(),
                                version: (e.document.version || 1) + 1,
                            },
                            contentChanges: [{ text: e.document.getText() }],
                        });
                    }
                }),
                vscode.workspace.onDidSaveTextDocument((doc) => {
                    if (doc.languageId === 'aurora') {
                        lspClient.sendNotificationToServer('textDocument/didSave', {
                            textDocument: { uri: doc.uri.toString() },
                        });
                    }
                }),
                vscode.workspace.onDidCloseTextDocument((doc) => {
                    if (doc.languageId === 'aurora') {
                        lspClient.sendNotificationToServer('textDocument/didClose', {
                            textDocument: { uri: doc.uri.toString() },
                        });
                    }
                })
            );

            // Open already visible aurora documents
            vscode.workspace.textDocuments.forEach((doc) => {
                if (doc.languageId === 'aurora') {
                    lspClient.sendNotificationToServer('textDocument/didOpen', {
                        textDocument: {
                            uri: doc.uri.toString(),
                            languageId: 'aurora',
                            version: 1,
                            text: doc.getText(),
                        }
                    });
                }
            });

            // Register completion provider
            context.subscriptions.push(
                vscode.languages.registerCompletionItemProvider('aurora', {
                    async provideCompletionItems(document, position) {
                        if (!lspClient || !lspClient.ready) return [];
                        try {
                            const result = await lspClient.sendRequest('textDocument/completion', {
                                textDocument: { uri: document.uri.toString() },
                                position: { line: position.line, character: position.character },
                                context: { triggerKind: 1 },
                            });
                            return (result.result && result.result.items || []).map(item => {
                                const ci = new vscode.CompletionItem(item.label);
                                ci.kind = item.kind;
                                ci.detail = item.detail;
                                ci.documentation = item.documentation;
                                ci.insertText = item.insertText || item.label;
                                return ci;
                            });
                        } catch { return []; }
                    }
                }, '.', '@', ' ')
            );

            // Register definition provider
            context.subscriptions.push(
                vscode.languages.registerDefinitionProvider('aurora', {
                    async provideDefinition(document, position) {
                        if (!lspClient || !lspClient.ready) return null;
                        try {
                            const result = await lspClient.sendRequest('textDocument/definition', {
                                textDocument: { uri: document.uri.toString() },
                                position: { line: position.line, character: position.character },
                            });
                            if (result.result && result.result.uri) {
                                return new vscode.Location(
                                    vscode.Uri.parse(result.result.uri),
                                    new vscode.Range(
                                        result.result.range.start.line, result.result.range.start.character,
                                        result.result.range.end.line, result.result.range.end.character
                                    )
                                );
                            }
                            return null;
                        } catch { return null; }
                    }
                })
            );

            // Register hover provider
            context.subscriptions.push(
                vscode.languages.registerHoverProvider('aurora', {
                    async provideHover(document, position) {
                        if (!lspClient || !lspClient.ready) return null;
                        try {
                            const result = await lspClient.sendRequest('textDocument/hover', {
                                textDocument: { uri: document.uri.toString() },
                                position: { line: position.line, character: position.character },
                            });
                            if (result.result && result.result.contents) {
                                return new vscode.Hover(
                                    new vscode.MarkdownString(result.result.contents)
                                );
                            }
                            return null;
                        } catch { return null; }
                    }
                })
            );

            // Register signature help provider
            context.subscriptions.push(
                vscode.languages.registerSignatureHelpProvider('aurora', {
                    async provideSignatureHelp(document, position) {
                        if (!lspClient || !lspClient.ready) return null;
                        try {
                            const result = await lspClient.sendRequest('textDocument/signatureHelp', {
                                textDocument: { uri: document.uri.toString() },
                                position: { line: position.line, character: position.character },
                            });
                            return result.result;
                        } catch { return null; }
                    }
                }, '(')
            );

            // Register references provider
            context.subscriptions.push(
                vscode.languages.registerReferenceProvider('aurora', {
                    async provideReferences(document, position) {
                        if (!lspClient || !lspClient.ready) return [];
                        try {
                            const result = await lspClient.sendRequest('textDocument/references', {
                                textDocument: { uri: document.uri.toString() },
                                position: { line: position.line, character: position.character },
                            });
                            return (result.result || []).map(ref => {
                                return new vscode.Location(
                                    vscode.Uri.parse(ref.uri),
                                    new vscode.Range(
                                        ref.range.start.line, ref.range.start.character,
                                        ref.range.end.line, ref.range.end.character
                                    )
                                );
                            });
                        } catch { return []; }
                    }
                })
            );

            // Register document symbol provider
            context.subscriptions.push(
                vscode.languages.registerDocumentSymbolProvider('aurora', {
                    async provideDocumentSymbols(document) {
                        if (!lspClient || !lspClient.ready) return [];
                        try {
                            const result = await lspClient.sendRequest('textDocument/documentSymbol', {
                                textDocument: { uri: document.uri.toString() },
                            });
                            return (result.result || []).map(sym => {
                                return new vscode.SymbolInformation(
                                    sym.name,
                                    parseInt(sym.kind) || vscode.SymbolKind.Variable,
                                    new vscode.Range(
                                        sym.range.start.line, sym.range.start.character,
                                        sym.range.end.line, sym.range.end.character
                                    ),
                                    document.uri
                                );
                            });
                        } catch { return []; }
                    }
                })
            );
        }
    });

    // Register semantic tokens (if LSP available)
    const semanticTokensLegend = new vscode.SemanticTokensLegend(
        [
            'variable', 'function', 'method', 'class', 'interface',
            'enum', 'struct', 'parameter', 'property', 'keyword',
            'comment', 'string', 'number', 'operator', 'type',
            'namespace', 'macro', 'modifier', 'event', 'decorator'
        ],
        [
            'declaration', 'definition', 'readonly', 'static',
            'abstract', 'deprecated', 'async', 'modification', 'documentation'
        ]
    );

    context.subscriptions.push(
        vscode.languages.registerDocumentSemanticTokensProvider('aurora', {
            async provideDocumentSemanticTokens(document) {
                if (!lspClient || !lspClient.ready) {
                    return new vscode.SemanticTokens(new Uint32Array(0));
                }
                try {
                    const result = await lspClient.sendRequest('textDocument/semanticTokens/full', {
                        textDocument: { uri: document.uri.toString() },
                    });
                    if (result.result && result.result.data) {
                        return new vscode.SemanticTokens(new Uint32Array(result.result.data));
                    }
                    return new vscode.SemanticTokens(new Uint32Array(0));
                } catch {
                    return new vscode.SemanticTokens(new Uint32Array(0));
                }
            }
        }, semanticTokensLegend)
    );

    // Register folding range provider
    context.subscriptions.push(
        vscode.languages.registerFoldingRangeProvider('aurora', {
            async provideFoldingRanges(document) {
                if (!lspClient || !lspClient.ready) return [];
                try {
                    const result = await lspClient.sendRequest('textDocument/foldingRange', {
                        textDocument: { uri: document.uri.toString() },
                    });
                    return (result.result || []).map(r => {
                        return new vscode.FoldingRange(r.startLine, r.endLine, r.kind);
                    });
                } catch { return []; }
            }
        })
    );

    // Register formatting provider
    context.subscriptions.push(
        vscode.languages.registerDocumentFormattingEditProvider('aurora', {
            async provideDocumentFormattingEdits(document) {
                if (!lspClient || !lspClient.ready) return [];
                try {
                    const result = await lspClient.sendRequest('textDocument/formatting', {
                        textDocument: { uri: document.uri.toString() },
                        options: { tabSize: 4, insertSpaces: true },
                    });
                    return (result.result || []).map(e => {
                        return new vscode.TextEdit(
                            new vscode.Range(
                                e.range.start.line, e.range.start.character,
                                e.range.end.line, e.range.end.character
                            ),
                            e.newText
                        );
                    });
                } catch { return []; }
            }
        })
    );

    // Register rename provider
    context.subscriptions.push(
        vscode.languages.registerRenameProvider('aurora', {
            async prepareRename(document, position) {
                return { range: document.getWordRangeAtPosition(position), placeholder: '' };
            },
            async provideRenameEdits(document, position, newName) {
                if (!lspClient || !lspClient.ready) return null;
                try {
                    const result = await lspClient.sendRequest('textDocument/rename', {
                        textDocument: { uri: document.uri.toString() },
                        position: { line: position.line, character: position.character },
                        newName: newName,
                    });
                    if (result.result && result.result.changes) {
                        const edit = new vscode.WorkspaceEdit();
                        for (const [uri, textEdits] of Object.entries(result.result.changes)) {
                            for (const te of textEdits) {
                                edit.replace(
                                    vscode.Uri.parse(uri),
                                    new vscode.Range(
                                        te.range.start.line, te.range.start.character,
                                        te.range.end.line, te.range.end.character
                                    ),
                                    te.newText
                                );
                            }
                        }
                        return edit;
                    }
                    return null;
                } catch { return null; }
            }
        })
    );

    // Register code actions
    context.subscriptions.push(
        vscode.languages.registerCodeActionsProvider('aurora', {
            async provideCodeActions(document, range, context) {
                if (!lspClient || !lspClient.ready) return [];
                try {
                    const diag = context.diagnostics && context.diagnostics.length > 0
                        ? context.diagnostics[0].message : '';
                    const result = await lspClient.sendRequest('textDocument/codeAction', {
                        textDocument: { uri: document.uri.toString() },
                        range: {
                            start: { line: range.start.line, character: range.start.character },
                            end: { line: range.end.line, character: range.end.character },
                        },
                        context: {
                            diagnostics: context.diagnostics.map(d => ({
                                range: {
                                    start: { line: d.range.start.line, character: d.range.start.character },
                                    end: { line: d.range.end.line, character: d.range.end.character },
                                },
                                message: d.message,
                                severity: d.severity,
                            })),
                        },
                    });
                    return (result.result || []).map(a => {
                        const action = new vscode.CodeAction(a.title);
                        action.kind = a.kind;
                        return action;
                    });
                } catch { return []; }
            }
        })
    );
}

function deactivate() {
    if (lspClient) {
        lspClient.stop();
        lspClient = null;
    }
}

module.exports = { activate, deactivate };
