import psana_legion

def analyze(event):
    run_fun1(event)
    run_fun2(event)

def filter(event):
    return True

psana_legion.start(analyze, filter)
