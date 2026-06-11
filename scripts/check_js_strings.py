import re

for name in ['left-pad_npm', 'moment_npm', 'uuid_npm']:
    with open(f'D:\\Downloads\\aurora_restructured\\{name}\\{name}.c', 'r', encoding='latin-1') as f:
        data = f.read()
    
    # Find the start of node_builtins_js value
    idx = data.find('node_builtins_js[]')
    # Find opening quote
    start = data.find('"', idx) + 1
    # The string is a C string literal with escape sequences
    # It ends at the first unescaped quote followed by semicolon in the array context
    # Actually, for C string literals, we can look for "; after the value
    # This is tricky because the string might contain escaped quotes
    end = data.find('";', start)
    js_str = data[start:end]
    
    # Handle C escape sequences for display
    js_str = js_str.replace('\\n', '\n').replace('\\t', '\t').replace('\\"', '"').replace("\\'", "'").replace('\\\\', '\\')
    
    print(f'{name}: {len(data[start:end])} raw bytes, decoded: {len(js_str)} chars')
    
    if name == 'left-pad_npm':
        ref = js_str
    else:
        print(f'  Equal to left-pad: {js_str == ref}')
