from __future__ import print_function

from psana import *

run_number = 310

ds = DataSource('exp=xpptut15:run=%s:smd' % run_number)
offs = []
for nevt, evt in enumerate(ds.events()):
    off = evt.get(EventOffset)
    print(nevt, off.offsets(), off.filenames())

    offs.append(off)

    if nevt > 10: break

ds2 = DataSource('exp=xpptut15:run=%s:rax' % run_number)
pnccd = Detector("pnccdFront", ds2.env())
xtcav = Detector("XrayTransportDiagnostic.0:Opal1000.0", ds2.env())

ds2.jump(offs[0].lastBeginCalibCycleFilename(), offs[0].lastBeginCalibCycleOffset())

for noff, off in enumerate(offs):
    evt = ds2.jump(off.filenames(), off.offsets())
    xtcavRaw = xtcav.raw(evt)
    pnccdRaw = pnccd.raw(evt)
    print()
    if xtcavRaw is not None: print(noff, 'xtcav', xtcavRaw.shape)
    if pnccdRaw is not None: print(noff, 'pnccd', pnccdRaw.shape)
