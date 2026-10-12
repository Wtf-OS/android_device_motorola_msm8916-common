from pathlib import Path
import json, subprocess
here = Path(__file__).resolve().parent
workspace = Path('/home/debian/osprey-port')
sources = workspace/'modem-tools'
pins = json.loads((here/'upstream.json').read_text())
for name, sha in pins.items():
    repo = sources/name
    assert subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip() == sha
rmtfs = sources/'rmtfs'
qrtr = sources/'qrtr'
text = (rmtfs/'rmtfs.c').read_text()
assert text.count('struct sigaction action;') == 1
text = text.replace('struct sigaction action;', 'setvbuf(stdout, NULL, _IONBF, 0);\n    struct sigaction action;')
# Upstream's unsigned-long format does not match int64_t on ARM32.
text = text.replace('=> 0x%lx (%d:%d)', '=> 0x%llx (%d:%d)').replace('caller_id, alloc_size, address, resp.result.result', 'caller_id, alloc_size, (unsigned long long)address, resp.result.result')
source = sources/'rmtfs-unbuffered.c'
source.write_text(text)
cc = workspace/'android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi31-clang'
args = [str(cc),'-O2','-static','-DANDROID','-D_GNU_SOURCE','-I'+str(qrtr/'include'),'-I'+str(qrtr/'lib'),'-I'+str(rmtfs),str(source)]
args += [str(rmtfs/name) for name in ['qmi_rmtfs.c','rproc.c','sharedmem.c','storage.c','util.c']]
args += [str(qrtr/'lib'/name) for name in ['qmi.c','qrtr.c','logging.c']]
subprocess.run(args+['-o',str(here/'osprey-rmtfs')],check=True)
