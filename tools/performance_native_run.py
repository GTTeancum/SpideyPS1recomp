"""Bounded native diagnostic runner for both games; process-local input only."""
import argparse, atexit, hashlib, json, os, pathlib, shutil, subprocess, time
import psutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'proof_render/performance-repairs'
p = argparse.ArgumentParser()
p.add_argument('game', choices=['sm1', 'sm2'])
p.add_argument('name')
p.add_argument('--scale', type=int, default=4)
p.add_argument('--backend', choices=['auto', 'gl33', 'gl45'], default='auto',
               help='Production renderer selection; legacy reference rendering requires a separate instrumented build')
p.add_argument('--shots', default='-1')
p.add_argument('--chase', action='store_true')
p.add_argument('--level', help='Select a normal level without the Chase follower or trigger pulses')
p.add_argument('--keep-boot', action='store_true', help='Let boot movies/sequences play instead of automatically pressing through them')
p.add_argument('--extra-script', default='', help='Append process-local button steps; never sends host input')
p.add_argument('--control-file', type=pathlib.Path, help='Opt-in process-local command file; no desktop input')
p.add_argument('--geometry', default='')
p.add_argument('--guard', type=lambda s: int(s, 16), help='Read-only native write watch at a guest RAM address (hex)')
p.add_argument('--guard-after', type=int, default=0, help='Start the bounded write watch after this presentation tick')
p.add_argument('--snapshots', default='', help='At most eight comma-separated native RAM snapshot ticks; route diagnostics, not timing acceptance')
p.add_argument('--snapshot-offsets', default='', help='SM1 only: positive checkpoint offsets after the explicit --level trigger archive loads')
p.add_argument('--gpu', action='store_true')
p.add_argument('--pad-trace', action='store_true', help='SM1 native pad decoding trace; bounded read-only diagnostics')
p.add_argument('--present-phases', action='store_true')
p.add_argument('--audible', action='store_true', help='Enable speaker playback; diagnostic runs otherwise mute only the output gain')
p.add_argument('--audio-state', action='store_true', help='Record the live audio queue and SPU state without recording desktop audio')
p.add_argument('--host-counters', action='store_true', help='Start bounded external host-counter and scheduling witnesses with the game')
p.add_argument('--host-processes', action='store_true', help='With --host-counters, record top process CPU consumers by name/PID only')
p.add_argument('--game-priority', choices=['inherit', 'normal'], default='inherit',
               help='Diagnostic Windows creation priority for this game only; collectors/parents and production launchers are unchanged')
p.add_argument('--executable', type=pathlib.Path, help='Use an explicit candidate EXE without replacing the canonical build')
p.add_argument('--native-script', default='', help='SM1 Chase-only native update:buttons:duration steps')
p.add_argument('--model', action='store_true')
p.add_argument('--native-textures', action='store_true', help='Diagnostic comparison without host texture replacements')
p.add_argument('--solid', default='')
p.add_argument('--ids', action='store_true')
p.add_argument('--suit', action='store_true', help='Use the bundled Magenta Man sample for this game')
p.add_argument('--trace', action='store_true')
p.add_argument('--stall-trace', action='store_true', help='Collect GC and managed contention events without CPU sampling')
p.add_argument('--jit-trace', action='store_true', help='Add JIT/loader events to --stall-trace for first-use hitch attribution')
p.add_argument('--trace-start', type=float, default=60)
p.add_argument('--trace-duration', type=int, default=30)
p.add_argument('--frames', type=int, default=6000)
p.add_argument('--wide', default='1')
a = p.parse_args()
if a.native_script and (a.game != 'sm1' or a.level != 'l5a1'):
    p.error('--native-script requires sm1 --level l5a1')
if a.jit_trace and not a.stall_trace:
    p.error('--jit-trace requires --stall-trace')
if a.host_processes and not a.host_counters:
    p.error('--host-processes requires --host-counters')
if a.pad_trace and a.game != 'sm1':
    p.error('--pad-trace is currently SM1 only')
snapshot_ticks = [int(t) for t in a.snapshots.split(',') if t.strip()]
snapshot_offsets = [int(t) for t in a.snapshot_offsets.split(',') if t.strip()]
if snapshot_offsets and (a.game != 'sm1' or not a.level or a.snapshots or a.geometry):
    p.error('Level-relative snapshots require SM1 and --level, without --snapshots or --geometry')
if len(snapshot_ticks) + len(snapshot_offsets) > 8 or any(t < 0 or t > a.frames for t in snapshot_ticks) or any(t <= 0 or t >= a.frames for t in snapshot_offsets):
    p.error('Use at most eight snapshot ticks within the run duration')
game = 'spiderman' if a.game == 'sm1' else 'spiderman2'
exe_name = 'SpiderMan.exe' if a.game == 'sm1' else 'SpiderMan2.exe'
source = ROOT / 'proof_render/performance-repairs' / a.game / exe_name
if a.executable:
    source = a.executable.resolve(strict=True)
run = OUT / a.name
if not run.resolve().is_relative_to(OUT.resolve()) or run.resolve() == OUT.resolve():
    raise SystemExit('Run name must resolve inside the performance evidence folder')
if run.exists():
    raise SystemExit('Use a fresh run folder; existing evidence must not be overwritten')
run.mkdir(parents=True)
if shutil.disk_usage(OUT).free < 3 * 1024**3:
    raise SystemExit('At least 3 GiB free is required for a diagnostic run')
cache = run / '.runtime'
if cache.exists():
    raise SystemExit('Use a fresh run folder; an earlier runtime cache is present')
proc = None
witnesses = []
def cleanup():
    if proc is not None and proc.poll() is None:
        proc.kill(); proc.wait()
    for witness, output in witnesses:
        try: witness.wait(timeout=3)
        except subprocess.TimeoutExpired: witness.kill(); witness.wait()
        output.close()
    for artifact in [run / exe_name] + [run / d for d in ['vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll']]:
        if artifact.is_file() and artifact.resolve().parent == run.resolve():
            artifact.unlink()
    if cache.exists() and cache.resolve().is_relative_to(run.resolve()) and cache.name == '.runtime':
        shutil.rmtree(cache)
atexit.register(cleanup)
exe = run / exe_name
if not exe.exists(): shutil.copy2(source, exe)
for dll in ['vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll']:
    if (source.parent/dll).exists(): shutil.copy2(source.parent/dll, run/dll)
(run/'interface.ini').write_text('[RecompOne]\nFullscreen=False\nWindowWidth=1280\nWindowHeight=960\nRenderScale=4\nFxaa=True\nVSync=False\nPanels.Output=True\n')
(run/'settings.json').write_text(json.dumps({'CdPath': str(ROOT/game/'extracted'), 'CardAEnabled':False, 'CardBEnabled':False, 'Widescreen':a.wide=='1', 'Muted':not a.audible}))
script = ('title.bmr+120:start:12;title.bmr+420:cross:12;title.bmr+720:cross:12' if a.game=='sm1' else
    'title.bmr+80:start:10;title.bmr+280:cross:10;title.bmr+480:cross:10;title.bmr+700:cross:10;e1m0_t.trg+1200:cross:8;e1m0_t.trg+1500:cross:8;e1m0_t.trg+1800:cross:8;e1m0_t.trg+2100:cross:8;e1m0_t.trg+2400:cross:8')
env = {k:v for k,v in os.environ.items() if not k.startswith(('SPIDEY_', 'RECOMP_', 'DOTNET_', 'COMPlus_'))}
env['DOTNET_BUNDLE_EXTRACT_BASE_DIR'] = str(cache)
env.update({'RECOMP_CAPTURE_HIDDEN':'1','RECOMP_RENDER_SCALE':str(a.scale),
    'SPIDEY_WIDE':a.wide,'SPIDEY_SCRIPT_EXCLUSIVE':'1','SPIDEY_SCRIPT':script,
    'SPIDEY_LEVEL':a.level or ('l5a3' if a.game=='sm1' else 'e1m0'),'SPIDEY_BOOT_SKIP_UNTIL':'title.bmr',
    'RECOMP_BACKEND':a.backend,'SPIDEY_SHOTS':a.shots,'SPIDEY_CAPTURE_PRESENTED':'1','SPIDEY_SHOT_DIR':str(run),'SPIDEY_EXIT':str(a.frames),'SPIDEY_LOG_DIR':str(run),
    'SPIDEY_STALL_EXIT':'1','SPIDEY_STALL':'20'})
if a.keep_boot:
    env.pop('SPIDEY_BOOT_SKIP_UNTIL', None)
if a.chase:
    env.update({'SPIDEY_LEVEL':'l5a1','SPIDEY_CHASE_FOLLOW':'1','SPIDEY_CHASE_REGION_PULSES':'1','SPIDEY_TRACE_TIMING':'1'})
if a.game == 'sm1' and env['SPIDEY_LEVEL'] == 'l5a1':
    env['SPIDEY_SCRIPT'] += ';title.bmr+1100:cross:12'
if a.extra_script: env['SPIDEY_SCRIPT'] += ';'+a.extra_script
if a.control_file: env['SPIDEY_CONTROL_FILE'] = str(a.control_file.resolve(strict=True))
if a.native_script: env['SPIDEY_NATIVE_SCRIPT'] = a.native_script
if a.guard is not None:
    env.update({'SPIDEY_GUARD':format(a.guard, 'x'), 'SPIDEY_GUARD_ALL':'1',
                'SPIDEY_GUARD_AFTER_FRAME':str(a.guard_after)})
if a.gpu: env['RECOMP_PERF_GPU']='1'
if a.pad_trace: env['SPIDEY_TRACE_PAD']='1'
if a.present_phases: env['RECOMP_PERF_PHASES']='1'
if a.audio_state: env['RECOMP_AUDIO_STATE_TRACE']=str(run/'audio-state.jsonl')
if a.solid: env['RECOMP_SOLID_GEOMETRY_CLUT']=a.solid
if a.ids: env['RECOMP_GEOMETRY_IDS']='1'
if a.suit:
    fixture = OUT / 'suit-fixtures' / a.game
    fixture.mkdir(parents=True, exist_ok=True)
    suit = fixture / 'magenta-man'
    if not suit.exists():
        shutil.copytree(ROOT / 'mods/samples' / ('magenta-man' if a.game == 'sm1' else 'magenta-man-sm2'), suit)
    (fixture / 'selected-suit.txt').write_text('magenta-man')
    env['SPIDEY_SUIT_MOD_DIR'] = str(fixture)
if a.model: env['RECOMP_MODEL_DUMP']=str(run/'model.jsonl')
if a.native_textures: env['RECOMP_NATIVE_TEXTURES']='1'
if a.geometry:
    first,last=a.geometry.split(':')
    env.update({'RECOMP_GEOMETRY_DUMP':str(run/'geometry.jsonl'),'RECOMP_GEOMETRY_START':first,'RECOMP_GEOMETRY_END':last,
        'SPIDEY_SNAP':last,'SPIDEY_SNAP_DIR':str(run),'SPIDEY_SNAP_EXTENDED':'1'})
if snapshot_ticks:
    if a.geometry: p.error('Use either geometry snapshots or explicit snapshots')
    env.update(SPIDEY_SNAP=','.join(map(str,snapshot_ticks)), SPIDEY_SNAP_DIR=str(run))
if snapshot_offsets:
    env.update(SPIDEY_SNAP=','.join(f'{a.level}_t.trg+{t}' for t in snapshot_offsets), SPIDEY_SNAP_DIR=str(run))
metadata = {'source_exe':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'game_creation_priority':a.game_priority,
    'speaker_playback':a.audible,
    'host_counters':a.host_counters,
    'host_processes':a.host_processes,
    'environment':{k:v for k,v in env.items() if k.startswith(('SPIDEY_','RECOMP_','DOTNET_','COMPlus_'))},
    'trace':a.trace,'stall_trace':a.stall_trace,'jit_trace':a.jit_trace,'trace_start':a.trace_start,'trace_duration':a.trace_duration,
    'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
(run/'environment.json').write_text(json.dumps(metadata,indent=2))
start=time.monotonic(); samples=[]; tracing=None; tracer_log=None
priority_observations=[]
def observe_priority(pid, role):
    row={'role':role,'pid':pid,'qpc_seconds':time.perf_counter()}
    try:
        process=psutil.Process(pid)
        priority=process.nice()  # Read only; never set scheduling policy.
        row.update(name=process.name(),created=process.create_time(),
                   priority_class=int(priority),priority_name=getattr(priority,'name',str(priority)))
    except (psutil.Error, OSError) as error:
        row['unavailable']=type(error).__name__
    priority_observations.append(row)
    (run/'process-priorities.json').write_text(json.dumps({
        'note':'Read-only point observations. Independent witnesses can share inherited priority; '
               'these samples do not establish historical or normal user launch priority.',
        'observations':priority_observations},indent=2))

observe_priority(os.getpid(),'runner')
try:
    for ancestor in psutil.Process().parents():
        observe_priority(ancestor.pid,'runner_ancestor')
except psutil.Error:
    pass
with (run/'console.log').open('w') as log:
    # Explicitly compare a normal desktop launch with inherited diagnostic
    # priority. This is a creation attribute of this child only, never an
    # adjustment to an existing process or a machine-wide scheduling policy.
    game_flags = subprocess.CREATE_NO_WINDOW
    if a.game_priority == 'normal':
        game_flags |= subprocess.NORMAL_PRIORITY_CLASS
    proc=subprocess.Popen([str(exe),str(ROOT/game/'extracted')],cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=game_flags)
    print(f'{a.name}: pid={proc.pid}',flush=True)
    observe_priority(proc.pid,'game')
    if a.host_counters:
        witness_tools = [('performance_host_counters.py','host-counters.jsonl'),
                         ('performance_heartbeat.py','heartbeat.jsonl')]
        if a.host_processes:
            witness_tools.append(('performance_host_processes.py','host-processes.jsonl'))
        for tool, filename in witness_tools:
            witness_log = (run/(filename+'.collector.log')).open('x')
            witness = subprocess.Popen([sys.executable, str(ROOT/'tools'/tool), str(proc.pid),
                str(run/filename), '--seconds', str(min(3600, max(155, a.frames/60+90)))],
                stdout=witness_log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            witnesses.append((witness, witness_log))
            observe_priority(witness.pid,tool)
    observed=psutil.Process(proc.pid)
    while proc.poll() is None and time.monotonic()-start<max(155,a.frames/60+90):
        if shutil.disk_usage(OUT).free < 2 * 1024**3:
            print('Stopping diagnostic: disk reserve reached', flush=True)
            break
        elapsed=time.monotonic()-start
        sample_start=time.perf_counter()
        try:
            mem=observed.memory_info(); cpu=observed.cpu_times()
            samples.append({'seconds':elapsed,'cpu':cpu.user+cpu.system,'working_set':mem.rss,'private':getattr(mem,'private',mem.vms),
                            'threads':[{'id':t.id,'cpu':t.user_time+t.system_time} for t in observed.threads()],
                            'qpc_seconds_start':sample_start,'qpc_seconds_end':time.perf_counter()})
        except psutil.NoSuchProcess: break
        if (a.trace or a.stall_trace) and tracing is None and elapsed >= a.trace_start:
            tracer_log=(run/'trace-collector.log').open('w')
            duration=f'00:{a.trace_duration//3600:02d}:{a.trace_duration//60%60:02d}:{a.trace_duration%60:02d}'
            trace_args=(['--providers','Microsoft-Windows-DotNETRuntime:0x4019:5' if a.jit_trace else 'Microsoft-Windows-DotNETRuntime:0x4001:4'] if a.stall_trace else
                        ['--profile','dotnet-sampled-thread-time','--providers','Microsoft-Windows-DotNETRuntime:0x4001:5','--format','Speedscope'])
            tracing=subprocess.Popen(['dotnet-trace','collect','--process-id',str(proc.pid),*trace_args,
                '--duration',duration,'--buffersize','64','--output',str(run/'cpu.nettrace')],
                stdout=tracer_log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
            observe_priority(tracing.pid,'runtime_trace_collector')
        time.sleep(.5)
    timed_out=proc.poll() is None
    if timed_out: proc.kill()
    proc.wait()
    if tracing is not None:
        try: tracing.wait(timeout=30)
        except subprocess.TimeoutExpired: tracing.kill(); tracing.wait()
        tracer_log.close()
for witness, output in witnesses:
    try: witness.wait(timeout=3)
    except subprocess.TimeoutExpired: witness.kill(); witness.wait()
    output.close()
result={'exit':proc.returncode,'timeout':timed_out,'seconds':time.monotonic()-start,'samples':samples,
        'trace_exit':None if tracing is None else tracing.returncode,
        'witness_exits':[witness.returncode for witness, _ in witnesses]}
(run/'result.json').write_text(json.dumps(result,indent=2))
print(json.dumps({k:v for k,v in result.items() if k!='samples'}),flush=True)



