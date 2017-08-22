import psana_legion

ds = LegionDataSource('asdf')

def filter(event):
    return True

ds.set_filter(filter)

for event in ds.events():
    run_fun1(event)
    run_fun2(event)
