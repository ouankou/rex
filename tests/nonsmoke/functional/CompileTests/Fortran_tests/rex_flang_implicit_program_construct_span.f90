type :: rex_implicit_payload
  integer :: value
end type rex_implicit_payload

type(rex_implicit_payload) :: payload
payload%value = 1
end
