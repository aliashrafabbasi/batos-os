.global isr_stub_0
.type isr_stub_0, @function

isr_stub_0:
    cli
    # Direct C handler ko call karo, wapas aane ki zaroorat hi nahi
    call c_exception_handler