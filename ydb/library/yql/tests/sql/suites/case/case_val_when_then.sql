select key, subkey, case value when subkey then "WAT" else value end as value from plato.Input;