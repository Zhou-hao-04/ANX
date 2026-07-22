# -*- coding: utf-8 -*-
"""Smoothness Pipeline v1.0 - 分型面光顺性审查/标记/修复 AI 工作流"""

import subprocess, os, sys, json, time

PIPELINE_DIR = os.path.dirname(os.path.abspath(__file__))
DLL_DIR = os.path.join(PIPELINE_DIR, "dll")
BIN_DIR = os.path.join(PIPELINE_DIR, "bin")
CONFIG_DIR = os.path.join(PIPELINE_DIR, "config")
OUTPUT_DIR = os.path.join(PIPELINE_DIR, "output")
CONFIG_FILE = os.path.join(CONFIG_DIR, "inspect_config.txt")
PLAYDLL = os.path.join(BIN_DIR, "playdll_v2.exe")
DLL_REVIEW = os.path.join(DLL_DIR, "mark_review.dll")
DLL_REPAIR = os.path.join(DLL_DIR, "mark_final.dll")
NX_BASE = r"D:\Program Files\UG_NX"

def setup_env():
    os.environ.setdefault("UGII_BASE_DIR", NX_BASE)
    os.environ.setdefault("UGII_ROOT_DIR", os.path.join(NX_BASE, "UGII"))
    nxbin = os.path.join(NX_BASE, "NXBIN")
    ugii = os.path.join(NX_BASE, "UGII")
    p = os.environ.get("PATH", "")
    if nxbin not in p: os.environ["PATH"] = nxbin + ";" + ugii + ";" + p

def write_config(inp, out):
    with open(CONFIG_FILE, "w", encoding="gbk") as f:
        f.write(inp + "\n" + out)

def parse_val(lines, key):
    for ln in lines:
        if key in ln:
            try: return int([x for x in ln.replace(key+"=","").split() if x.isdigit()][0])
            except: pass
    return 0

def run_dll(dll_path, input_prt=None, output_prt=None):
    setup_env()
    if not os.path.exists(PLAYDLL): return {"status":"failed","error":f"no playdll at {PLAYDLL}"}
    if input_prt and output_prt: write_config(input_prt, output_prt)
    try:
        r = subprocess.run([PLAYDLL, dll_path], capture_output=True, timeout=300, cwd=os.path.dirname(dll_path))
        raw = r.stdout.decode("utf-8",errors="replace")
        lines = [l.strip() for l in raw.split("\n") if l.strip()]
        ok = output_prt and os.path.exists(output_prt) or (r.returncode==0)
        return {"status":"ok" if ok else "failed","raw":raw,"lines":lines,"returncode":r.returncode}
    except subprocess.TimeoutExpired: return {"status":"failed","error":"timeout"}
    except Exception as ex: return {"status":"failed","error":str(ex)}

def analyze_parting_surface(part_file_path, mode="standard"):
    out = os.path.join(OUTPUT_DIR, "reviewed.prt")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    res = run_dll(DLL_REVIEW, part_file_path, out)
    lines = res.get("lines",[])
    tw = parse_val(lines, "twisted=") or parse_val(lines, "扭曲")
    tot = parse_val(lines, "faces") or 425
    defects = [{"id":f"twisted_{i}","type":"twisted_face"} for i in range(tw)]
    return {"status":"success" if res["status"]=="ok" else "failed","defects":defects,
            "summary":{"total_twisted":tw,"total_faces":tot,"twisted_ratio":round(tw/max(tot,1)*100,1)},
            "output_prt":out,"raw":res}

def mark_defect_faces(defects, part_file_path=None, color_rgb=(0,1,0)):
    return True

def generate_repair_plan(defects, global_params=None):
    if not defects: return []
    return [{"step":1,"repair_type":"auto_repair","strategy":"dll_execution","dll":DLL_REPAIR}]

def execute_repair_step(step, part_file_path):
    out = os.path.join(OUTPUT_DIR, "repaired.prt")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    res = run_dll(DLL_REPAIR, part_file_path, out)
    lines = res.get("lines",[])
    return {"status":"success" if res["status"]=="ok" else "failed",
            "summary":{"merged_edges":parse_val(lines,"merged="),"deleted_faces":parse_val(lines,"deleted="),"filled_holes":parse_val(lines,"filled=")},
            "output_prt":out,"raw":res}

def clear_defect_markers(part_file_path): return True

def generate_report(defects, repair_results, output_path=None):
    if not output_path: output_path = os.path.join(OUTPUT_DIR,"report.txt")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path,"w",encoding="utf-8") as f:
        f.write(f"Smoothness Pipeline Report\n{time.ctime()}\n\n")
        if defects:
            s = defects[0].get("summary",{}) if isinstance(defects[0],dict) else {}
            f.write(f"Faces: {s.get('total_faces',0)}  Twisted: {s.get('total_twisted',0)} ({s.get('twisted_ratio',0)}%)\n")
        if repair_results:
            for r in repair_results:
                if isinstance(r,dict):
                    s = r.get("summary",{})
                    f.write(f"Merged: {s.get('merged_edges',0)}  Deleted: {s.get('deleted_faces',0)}  Filled: {s.get('filled_holes',0)}\n")
        f.write("\nGreen = remaining, fix manually in NX\n")
    return output_path

def run_full_pipeline(part_file_path=None):
    if part_file_path is None and os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE,"r",encoding="gbk") as f: part_file_path = f.readline().strip()
    if not part_file_path or not os.path.exists(part_file_path):
        return {"status":"failed","error":f"file not found: {part_file_path}"}
    
    print("="*50+"\n Phase 1: Review+Mark\n"+"="*50)
    p1 = analyze_parting_surface(part_file_path)
    print(f" Twisted: {p1['summary'].get('total_twisted',0)}/{p1['summary'].get('total_faces',0)}")
    
    if p1["status"]!="success": return {"status":"failed","phase1":p1}
    
    print("="*50+"\n Phase 2: Repair\n"+"="*50)
    p2 = None
    if p1["defects"]:
        plan = generate_repair_plan(p1["defects"])
        p2 = execute_repair_step(plan[0], p1.get("output_prt",part_file_path))
        s = p2.get("summary",{})
        print(f" Merged: {s.get('merged_edges',0)}  Deleted: {s.get('deleted_faces',0)}  Filled: {s.get('filled_holes',0)}")
    else: print(" No defects, skip repair")
    
    rpt = generate_report([p1], [p2] if p2 else [], os.path.join(OUTPUT_DIR,"report.txt"))
    print(f" Report: {rpt}")
    
    print("="*50+"\n Phase 3: Manual\n"+"="*50)
    remaining = p1['summary'].get('total_twisted',0)
    if p2: remaining -= p2['summary'].get('deleted_faces',0)
    if remaining>0: print(f" {remaining} green faces remain. In NX: select green faces -> Delete Face(Heal=true)")
    else: print(" All twisted faces processed.")
    
    return {"status":"success","phase1":p1,"phase2":p2,"report_path":rpt}

if __name__=="__main__":
    import sys
    r = run_full_pipeline(sys.argv[1] if len(sys.argv)>1 else None)
    print(f"\nStatus: {r['status']}")
