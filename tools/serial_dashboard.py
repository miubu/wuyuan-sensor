import csv
import json
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import serial
from serial.tools import list_ports


class Dashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("RF430 USB 测量仪")
        self.root.geometry("1040x700")
        self.port = None
        self.messages = queue.Queue()
        self.rows = []
        self.points = []
        self.status_text = tk.StringVar(value="正在寻找 ESP32…")
        self.uid_text = tk.StringVar(value="UID —")
        self.resistance = tk.StringVar(value="— Ω")
        self.raw = tk.StringVar(value="ADC1 —    ADC2 —")
        self.delta = tk.StringVar(value="ΔR —    ΔR/R0 —    应变 — με")
        self.build_ui()
        threading.Thread(target=self.serial_worker, daemon=True).start()
        self.root.after(50, self.process_messages)

    def build_ui(self):
        style = ttk.Style(); style.theme_use("clam")
        self.root.configure(bg="#08131c")
        top = tk.Frame(self.root, bg="#10212e", padx=18, pady=14); top.pack(fill="x")
        tk.Label(top, text="RF430 USB SENSOR LAB", bg="#10212e", fg="#55d8c7", font=("Segoe UI",18,"bold")).pack(side="left")
        tk.Label(top, textvariable=self.status_text, bg="#10212e", fg="#eaf4f8", font=("Segoe UI",11)).pack(side="right")
        body=tk.Frame(self.root,bg="#08131c",padx=18,pady=14);body.pack(fill="both",expand=True)
        info=tk.Frame(body,bg="#10212e",padx=16,pady=12);info.pack(fill="x")
        tk.Label(info,textvariable=self.uid_text,bg="#10212e",fg="#9bb0bd").pack(anchor="w")
        tk.Label(info,textvariable=self.resistance,bg="#10212e",fg="white",font=("Consolas",28,"bold")).pack(anchor="w",pady=6)
        tk.Label(info,textvariable=self.raw,bg="#10212e",fg="#55d8c7",font=("Consolas",13)).pack(anchor="w")
        tk.Label(info,textvariable=self.delta,bg="#10212e",fg="#ffb45c",font=("Consolas",13)).pack(anchor="w",pady=(4,0))
        controls=tk.Frame(body,bg="#08131c");controls.pack(fill="x",pady=12)
        for text,cmd in [("连续采样","start"),("停止","stop"),("单次采样","single"),("设置 R0","baseline")]:
            tk.Button(controls,text=text,command=lambda c=cmd:self.send(c),bg="#167b73",fg="white",relief="flat",padx=15,pady=8).pack(side="left",padx=(0,8))
        tk.Button(controls,text="清空曲线",command=self.clear,bg="#283f50",fg="white",relief="flat",padx=15,pady=8).pack(side="left",padx=(0,8))
        tk.Button(controls,text="导出 CSV",command=self.export,bg="#283f50",fg="white",relief="flat",padx=15,pady=8).pack(side="left")
        tk.Label(controls,text="采样速度",bg="#08131c",fg="#9bb0bd").pack(side="left",padx=(20,6))
        self.speed=ttk.Combobox(controls,state="readonly",width=15,values=("最快（约1秒）","快速（1.5秒）","标准（2秒）","慢速（5秒）"))
        self.speed.current(1);self.speed.pack(side="left");self.speed.bind("<<ComboboxSelected>>",self.set_speed)
        config=tk.Frame(body,bg="#08131c");config.pack(fill="x",pady=(0,8))
        tk.Label(config,text="参考电阻 Ω",bg="#08131c",fg="#9bb0bd").pack(side="left");self.rref=tk.Entry(config,width=12);self.rref.insert(0,"200000");self.rref.pack(side="left",padx=6)
        tk.Button(config,text="应用",command=lambda:self.send("rref "+self.rref.get())).pack(side="left")
        tk.Label(config,text="Gauge Factor",bg="#08131c",fg="#9bb0bd").pack(side="left",padx=(20,0));self.gf=tk.Entry(config,width=8);self.gf.insert(0,"2.0");self.gf.pack(side="left",padx=6)
        tk.Button(config,text="应用",command=lambda:self.send("gf "+self.gf.get())).pack(side="left")
        self.canvas=tk.Canvas(body,bg="#061019",highlightthickness=1,highlightbackground="#294354");self.canvas.pack(fill="both",expand=True)

    def find_port(self):
        ports=list(list_ports.comports())
        for p in ports:
            if p.vid==0x303A: return p.device
        return ports[0].device if ports else None

    def serial_worker(self):
        while True:
            try:
                if not self.port or not self.port.is_open:
                    name=self.find_port()
                    if not name: self.messages.put(("status","未找到串口"));time.sleep(1);continue
                    self.port=serial.Serial(name,115200,timeout=.4);self.messages.put(("status",f"已连接 {name}"))
                line=self.port.readline().decode("utf-8",errors="replace").strip()
                if line.startswith("JSON "): self.messages.put(("json",json.loads(line[5:])))
            except Exception as exc:
                self.messages.put(("status",f"串口断开：{exc}"))
                try:self.port.close()
                except Exception:pass
                self.port=None;time.sleep(1)

    def send(self, command):
        try:self.port.write((command+"\n").encode())
        except Exception:messagebox.showwarning("RF430","串口尚未连接")

    def set_speed(self, _event=None):
        periods=(650,1500,2000,5000)
        self.send(f"interval {periods[self.speed.current()]}")

    def process_messages(self):
        while not self.messages.empty():
            kind,data=self.messages.get()
            if kind=="status":self.status_text.set(data)
            elif data.get("type")=="status":
                self.status_text.set(data.get("message",""));self.uid_text.set("UID "+(data.get("uid") or "—"))
            elif data.get("type")=="measurement":self.update_measurement(data)
        self.root.after(50,self.process_messages)

    def update_measurement(self,d):
        if not d.get("valid"):
            self.status_text.set("测量失败："+d.get("error","未知错误"));return
        self.resistance.set(f"{d['sensor_ohm']:,.2f} Ω")
        self.raw.set(f"ADC1 {d['adc1_raw']}    ADC2 {d['adc2_raw']}")
        self.delta.set(f"ΔR {d['delta_r_ohm']:.3f} Ω    ΔR/R0 {d['relative_change']:.7f}    应变 {d['strain_ue']:.2f} με")
        self.rows.append(d);self.points.append(float(d["sensor_ohm"]));self.points=self.points[-500:];self.draw()

    def draw(self):
        c=self.canvas;c.delete("all");w=max(c.winfo_width(),10);h=max(c.winfo_height(),10)
        if len(self.points)<2:return
        lo,hi=min(self.points),max(self.points);hi=hi if hi>lo else lo+1
        xy=[]
        for i,v in enumerate(self.points):xy.extend((i/(len(self.points)-1)*(w-20)+10,h-10-(v-lo)/(hi-lo)*(h-30)))
        c.create_line(*xy,fill="#55d8c7",width=2);c.create_text(12,10,text=f"{hi:.2f} Ω",fill="#9bb0bd",anchor="nw");c.create_text(12,h-8,text=f"{lo:.2f} Ω",fill="#9bb0bd",anchor="sw")

    def clear(self):self.rows.clear();self.points.clear();self.draw()
    def export(self):
        if not self.rows:return
        name=filedialog.asksaveasfilename(defaultextension=".csv",filetypes=[("CSV","*.csv")])
        if name:
            with open(name,"w",newline="",encoding="utf-8-sig") as f:writer=csv.DictWriter(f,fieldnames=self.rows[0].keys());writer.writeheader();writer.writerows(self.rows)


if __name__ == "__main__":
    root=tk.Tk();Dashboard(root);root.mainloop()
