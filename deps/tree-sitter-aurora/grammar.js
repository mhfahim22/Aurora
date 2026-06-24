/**
 * @file tree-sitter-aurora/grammar.js
 * Tree-sitter grammar for the Aurora programming language
 * Based on the compiler's lexer + parser specification
 */

module.exports = grammar({
    name: 'aurora',

    extras: $ => [
        /\s/,
        $.comment,
    ],

    conflicts: $ => [],

    word: $ => $.identifier,

    rules: {
        source_file: $ => repeat($._definition),

        _definition: $ => choice(
            $.function_definition,
            $.class_definition,
            $.struct_definition,
            $.enum_definition,
            $.interface_definition,
            $.extern_definition,
            $.import_statement,
            $.statement,
        ),

        /* ── Comments ── */
        comment: $ => choice(
            token(seq('#', /.*/)),
            token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/')),
        ),

        /* ── Literals ── */
        string_literal: $ => choice(
            seq('"', token(choice(/[^"\\]+/, /\\./)), '"'),
            seq("'", token(choice(/[^'\\]+/, /\\./)), "'"),
        ),

        number_literal: $ => token(choice(
            /\d+\.\d+(?:[eE][+-]?\d+)?/,
            /\d+/,
        )),

        boolean_literal: $ => choice('true', 'false'),
        null_literal: $ => 'null',

        /* ── Identifiers ── */
        identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

        /* ── Attributes ── */
        attribute: $ => seq('@', $.identifier),

        /* ── Types ── */
        type: $ => choice(
            'int', 'float', 'string', 'bool', 'void',
            'list', 'map', 'set', 'array', 'json',
            'tuple', 'vector', 'stack', 'queue',
            $.identifier,
        ),

        /* ── Function definition ── */
        function_definition: $ => seq(
            choice('function', 'extern'),
            optional(seq('"', /[^"]*/, '"')),
            field('name', $.identifier),
            '(',
            optional($.parameter_list),
            ')',
            optional(seq(':', $.type)),
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        parameter_list: $ => seq(
            $.parameter,
            repeat(seq(',', $.parameter)),
            optional(','),
        ),

        parameter: $ => seq(
            field('name', $.identifier),
            optional(seq(':', $.type)),
        ),

        /* ── Class definition ── */
        class_definition: $ => seq(
            optional(choice('abstract', 'final')),
            'class',
            field('name', $.identifier),
            optional(seq('extends', $.identifier)),
            optional(seq('implements', $.identifier, repeat(seq(',', $.identifier)))),
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        /* ── Struct / Enum / Interface ── */
        struct_definition: $ => seq(
            'struct',
            field('name', $.identifier),
            choice(
                seq('{', repeat($.field), '}'),
                seq(':', repeat(choice($.field, $.function_definition))),
            ),
        ),

        enum_definition: $ => seq(
            'enum',
            field('name', $.identifier),
            choice(
                seq('{', repeat($.enum_variant), '}'),
                seq(':', repeat($.enum_variant)),
            ),
        ),

        enum_variant: $ => seq(
            field('name', $.identifier),
            optional(seq('(', $.parameter_list, ')')),
        ),

        interface_definition: $ => seq(
            'interface',
            field('name', $.identifier),
            choice(
                seq('{', repeat($.function_definition), '}'),
                seq(':', repeat($.function_definition)),
            ),
        ),

        field: $ => seq(
            field('name', $.identifier),
            optional(seq(':', $.type)),
            optional(seq('=', $.expression)),
        ),

        /* ── Extern ── */
        extern_definition: $ => seq(
            'extern',
            optional(seq('"', /[^"]*/, '"')),
            choice(
                seq('function', $.identifier, '(', optional($.parameter_list), ')', optional(seq(':', $.type))),
                seq(choice('struct', 'union'), $.identifier, '{', repeat($.field), '}'),
            ),
        ),

        /* ── Import ── */
        import_statement: $ => seq(
            'import',
            choice(
                seq($.identifier, repeat(seq('.', $.identifier))),
                seq('from', string(0, $.string_literal)),
            ),
        ),

        /* ── Statements ── */
        statement: $ => choice(
            $.return_statement,
            $.if_statement,
            $.while_statement,
            $.for_statement,
            $.match_statement,
            $.try_statement,
            $.assignment,
            $.expression_statement,
            $.output_statement,
            $.pass_statement,
        ),

        pass_statement: $ => 'pass',

        return_statement: $ => seq('return', optional($.expression)),

        if_statement: $ => seq(
            'if', $.expression,
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
            repeat($.elseif_clause),
            optional($.else_clause),
        ),

        elseif_clause: $ => seq(
            'elseif', $.expression,
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        else_clause: $ => seq(
            'else',
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        while_statement: $ => seq(
            'while', $.expression,
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        for_statement: $ => seq(
            'for',
            field('variable', $.identifier),
            'in',
            $.expression,
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
        ),

        match_statement: $ => seq(
            'match', $.expression,
            choice(
                seq('{', repeat($.match_case), '}'),
                seq(':', repeat($.match_case)),
            ),
        ),

        match_case: $ => seq(
            'case', $.expression, ':', repeat($._definition),
        ),

        try_statement: $ => seq(
            'try',
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', repeat($._definition)),
            ),
            optional(seq('catch', $.identifier, ':', repeat($._definition))),
            optional(seq('finally', ':', repeat($._definition))),
        ),

        output_statement: $ => seq('output', '(', $.expression, ')'),

        assignment: $ => seq(
            $.expression,
            '=',
            $.expression,
        ),

        expression_statement: $ => $.expression,

        /* ── Expressions ── */
        expression: $ => choice(
            $.binary_expression,
            $.unary_expression,
            $.call_expression,
            $.index_expression,
            $.attribute_expression,
            $.lambda_expression,
            $.primary_expression,
        ),

        primary_expression: $ => choice(
            $.identifier,
            $.string_literal,
            $.number_literal,
            $.boolean_literal,
            $.null_literal,
            $.array_literal,
            $.struct_literal,
            seq('(', $.expression, ')'),
        ),

        array_literal: $ => seq(
            '[',
            optional(seq($.expression, repeat(seq(',', $.expression)))),
            ']',
        ),

        struct_literal: $ => seq(
            $.identifier,
            '{',
            optional(seq($.field_initializer, repeat(seq(',', $.field_initializer)))),
            '}',
        ),

        field_initializer: $ => seq(
            field('name', $.identifier),
            ':',
            $.expression,
        ),

        call_expression: $ => seq(
            $.expression,
            '(',
            optional(seq($.expression, repeat(seq(',', $.expression)))),
            ')',
        ),

        index_expression: $ => seq(
            $.expression,
            '[',
            $.expression,
            ']',
        ),

        attribute_expression: $ => seq(
            $.expression,
            '.',
            $.identifier,
        ),

        lambda_expression: $ => seq(
            'lambda',
            optional(seq('(', optional($.parameter_list), ')')),
            optional(seq(':', $.type)),
            choice(
                seq('{', repeat($._definition), '}'),
                seq(':', $.expression),
            ),
        ),

        unary_expression: $ => seq(
            choice('-', '!', 'not', '~'),
            $.expression,
        ),

        binary_expression: $ => {
            const table = [
                [choice('and', 'or', 'xor')],
                [choice('==', '!=', '<', '>', '<=', '>=', 'equals')],
                ['in'],
                [choice('+', '-')],
                [choice('*', '/', '%', '//')],
                [choice('<<', '>>')],
                ['&'],
                ['^'],
                ['|'],
                ['..', '...'],
                [choice('as', 'is')],
            ];

            return choice(...table.map(([operator, ...rest]) => {
                const others = rest.length > 0 ? choice(operator, ...rest) : operator;
                return prec.left(seq($.expression, others, $.expression));
            }));
        },

        /* ── Keywords (referenced for highlighting) ── */
        keyword: $ => choice(
            'function', 'return', 'if', 'elseif', 'else', 'while', 'for', 'loop',
            'break', 'continue', 'skip', 'match', 'case', 'default', 'switch',
            'class', 'extends', 'implements', 'interface', 'enum', 'struct',
            'public', 'private', 'protected', 'static', 'final', 'abstract',
            'try', 'catch', 'finally', 'throw', 'panic', 'ensure',
            'import', 'from', 'namespace', 'module', 'package', 'extern',
            'async', 'await', 'spawn', 'parallel', 'thread',
            'true', 'false', 'null',
            'and', 'or', 'not', 'xor', 'in', 'is', 'as',
            'new', 'self', 'super', 'lambda', 'type',
            'output', 'debug', 'log', 'pass',
            'move', 'copy', 'shared', 'weak', 'borrow', 'drop', 'delete',
            'safe', 'unsafe', 'const', 'mutable', 'reference', 'pointer',
            'event', 'signal', 'emit', 'callback',
            'scene', 'entity', 'sprite', 'camera', 'physics',
            'component', 'render', 'state', 'properties',
            'server', 'api', 'route', 'middleware',
            'ai', 'tensor', 'train', 'predict',
            'int', 'float', 'string', 'bool', 'void',
            'list', 'map', 'set', 'array', 'json',
        ),
    },
});
