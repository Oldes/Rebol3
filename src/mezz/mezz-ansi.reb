Rebol [
	Title:   "ANSI escape sequences support"
	File:    %mezz-ansi.reb
	Version: 1.3.0
	Date:    21-Apr-2026
	Purpose: "Decorate any value with basic ANSI color sequences"
]

as-gray:   function/with["Decorates a value with gray ANSI escape codes"   value return: [string!]][ajoin [ansi/gray           value ansi/reset]] :system/options
as-red:    function/with["Decorates a value with red ANSI escape codes"    value return: [string!]][ajoin [ansi/bright-red     value ansi/reset]] :system/options
as-green:  function/with["Decorates a value with green ANSI escape codes"  value return: [string!]][ajoin [ansi/bright-green   value ansi/reset]] :system/options
as-yellow: function/with["Decorates a value with yellow ANSI escape codes" value return: [string!]][ajoin [ansi/bright-yellow  value ansi/reset]] :system/options
as-blue:   function/with["Decorates a value with blue ANSI escape codes"   value return: [string!]][ajoin [ansi/bright-blue    value ansi/reset]] :system/options
as-purple: function/with["Decorates a value with purple ANSI escape codes" value return: [string!]][ajoin [ansi/bright-magenta value ansi/reset]] :system/options
as-cyan:   function/with["Decorates a value with cyan ANSI escape codes"   value return: [string!]][ajoin [ansi/bright-cyan    value ansi/reset]] :system/options
as-white:  function/with["Decorates a value with white ANSI escape codes"  value return: [string!]][ajoin [ansi/bright-white   value ansi/reset]] :system/options
