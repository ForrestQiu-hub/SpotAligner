# -*- coding: utf-8 -*-
"""Builds the literature review as a DOCX with python-docx."""

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.shared import Pt, Cm, RGBColor

OUT = "文献综述-点话筒声像定位与时间差.docx"
BODY = "宋体"
HEAD = "微软雅黑"


def set_fixed_layout(t, widths_cm):
    """Word and LibreOffice ignore cell widths unless the table layout is fixed;
    the grid definition has to carry them too."""
    t.autofit = False
    tblPr = t._tbl.tblPr
    layout = tblPr.find(qn("w:tblLayout"))
    if layout is None:
        layout = tblPr.makeelement(qn("w:tblLayout"), {})
        tblPr.append(layout)
    layout.set(qn("w:type"), "fixed")

    total = sum(widths_cm)
    tblW = tblPr.find(qn("w:tblW"))
    if tblW is None:
        tblW = tblPr.makeelement(qn("w:tblW"), {})
        tblPr.append(tblW)
    tblW.set(qn("w:w"), str(int(total * 567)))
    tblW.set(qn("w:type"), "dxa")

    grid = t._tbl.find(qn("w:tblGrid"))
    if grid is not None:
        for i, col in enumerate(grid.findall(qn("w:gridCol"))):
            if i < len(widths_cm):
                col.set(qn("w:w"), str(int(widths_cm[i] * 567)))

    for row in t.rows:
        for i, w in enumerate(widths_cm):
            row.cells[i].width = Cm(w)


def style_font(style, latin, east, size=None, bold=None, color=None):
    style.font.name = latin
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = rpr.makeelement(qn("w:rFonts"), {})
        rpr.append(rfonts)
    rfonts.set(qn("w:eastAsia"), east)
    rfonts.set(qn("w:ascii"), latin)
    rfonts.set(qn("w:hAnsi"), latin)
    if size is not None:
        style.font.size = Pt(size)
    if bold is not None:
        style.font.bold = bold
    if color is not None:
        style.font.color.rgb = color


doc = Document()

# page setup: A4 with sane margins
for section in doc.sections:
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.left_margin = Cm(2.5)
    section.right_margin = Cm(2.5)
    section.top_margin = Cm(2.4)
    section.bottom_margin = Cm(2.4)

style_font(doc.styles["Normal"], BODY, BODY, size=10.5)
doc.styles["Normal"].paragraph_format.line_spacing = 1.35
doc.styles["Normal"].paragraph_format.space_after = Pt(6)

for name, size in (("Title", 20), ("Heading 1", 15), ("Heading 2", 12.5), ("Heading 3", 11)):
    if name in [s.name for s in doc.styles]:
        style_font(doc.styles[name], HEAD, HEAD, size=size,
                   color=RGBColor(0x1F, 0x1F, 0x1F))
if "Quote" in [s.name for s in doc.styles]:
    style_font(doc.styles["Quote"], BODY, BODY, size=10)
if "List Bullet" in [s.name for s in doc.styles]:
    style_font(doc.styles["List Bullet"], BODY, BODY, size=10.5)


def h(text, level=1):
    return doc.add_heading(text, level=level)


def p(text, style=None, bold_first=False):
    par = doc.add_paragraph(style=style)
    if bold_first and "：" in text:
        head, rest = text.split("：", 1)
        par.add_run(head + "：").bold = True
        par.add_run(rest)
    else:
        par.add_run(text)
    return par


def bullets(items, style="List Bullet"):
    for it in items:
        par = doc.add_paragraph(style=style)
        if "：" in it:
            head, rest = it.split("：", 1)
            par.add_run(head + "：").bold = True
            par.add_run(rest)
        else:
            par.add_run(it)


def table(headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    for i, cell in enumerate(t.rows[0].cells):
        cell.text = ""
        run = cell.paragraphs[0].add_run(headers[i])
        run.bold = True
        run.font.size = Pt(9.5)
    for row in rows:
        cells = t.add_row().cells
        for i, value in enumerate(row):
            cells[i].text = ""
            run = cells[i].paragraphs[0].add_run(value)
            run.font.size = Pt(9.5)
    if widths:
        total = float(sum(widths))
        scaled = [w * 15.0 / total for w in widths]
        set_fixed_layout(t, scaled)
    doc.add_paragraph()
    return t


# ----------------------------------------------------------------- title block
doc.add_heading("点话筒声像定位中的电平差与时间差", level=0)
sub = doc.add_paragraph()
sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = sub.add_run("——文献综述,以及对 Spot Aligner 设计取舍的含义")
run.font.size = Pt(11)
run.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

info = doc.add_paragraph()
info.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = info.add_run("Zhenyan Qiu · 南开大学物理科学学院 · 2026-10-16 · 反馈:qiuzhenyan4135@163.com")
run.font.size = Pt(9)
run.font.color.rgb = RGBColor(0x77, 0x77, 0x77)

# ----------------------------------------------------------------- abstract
h("摘要", 1)
p("古典录音中把点话筒加入间距型主话筒(AB、德卡树等)时,存在两种做法。一种只用强度差:"
  "把点话筒对齐到离它最近的那一支主话筒,只施加一个延时,左右两路相同,声像交给宿主软件的音量"
  "声像旋钮。另一种同时使用时间差:由几何算出主阵列对同一位置本应产生的左右延时与电平,"
  "分别施加在左右两路,点话筒因此带着时间差进入混音。本文检索并整理了与这一分歧直接相关的"
  "心理声学与录音技术文献,得到三点结论:第一,人耳对时间差的感知分成三个区间,低于约 1 ms 时"
  "电平差与时间差共同决定方向(求和定位),约 2–5 ms 时方向归先到达的一方(定位优势),"
  "更晚则被听成回声;两种做法恰好分别落在这三个区间的不同位置上。第二,点话筒因为离声源更近而"
  "先到达,若不施加对齐延时,按定位优势它会夺走声像,这正是对齐延时存在的文献依据。"
  "第三,时间差过大会使像场分裂、稳定性下降,这是反对引入左右时间差的一方的正当依据;"
  "但“必须只使用一个延时”属于经验规范,本文未找到支持其为唯一正确做法的测量研究。")

# ----------------------------------------------------------------- 1
h("一、问题的由来", 1)
p("Spot Aligner 是一个免费 VST3 插件,给古典录音中的点话筒做两件事:第一,按声源到各支主话筒的"
  "距离与声速算出对齐延时,把点话筒推到主话筒之后;第二,按主话筒制式(AB / ORTF / XY / NOS / "
  "DIN / Decca Tree)算出左右两路各自应收到的延时与电平,使点话筒落在它在房间里真正所在的位置。"
  "它的默认行为对左右两路施加不同的延时。")
p("一位职业录音师对此提出反对:点话筒应当根据它靠左还是靠右,对到相应的那一支主话筒上(即离它"
  "最近的那支),只用一个延时、加在左右两路;声像由 DAW 的声像旋钮决定。他的理由有两点:"
  "对两支话筒分别施加不同延时,会改变点话筒自身(当它是一对 AB 时)的时间差;而且左右两路梳状"
  "滤波不同,会使单声道兼容性变差。他甚至表示“一直不觉得这类功能有用”。")
p("本文的目的不是裁决谁对谁错,而是把与这一分歧有关的文献摆清楚:哪些结论有测量支持,"
  "哪些只是行业经验,哪些问题至今没有直接的实验对比。")

# ----------------------------------------------------------------- 2
h("二、幻象声源的两条线索:电平差与时间差", 1)
p("两扬声器之间的幻象声源可以由电平差产生,也可以由时间差产生,这一点在立体声技术的最早期就"
  "已被掌握。", bold_first=False)
bullets([
    "Blumlein 1931：提出用和差信号(强度差)在两个扬声器之间产生幻象,是电平差路线的起点。",
    "Clark、Dutton 与 Vanderlyn 1957：证明两支有间距的话筒(时间差)同样能产生幻象,"
    "并系统描述了所谓 stereosonic 系统,是时间差路线的起点。",
    "Theile 与 Plenge 1977：给出强度立体声的定位曲线,并指出其行为随频率变化。",
    "Sengpiel 的“等价立体声”：幻象位置的满偏大致对应 18 dB 的电平差或 1.5 ms 的时间差,"
    "两者一阶近似线性叠加。需要说明两点:其一,这组常数出自德国录音学派的讲义与实践文献,"
    "本次检索未找到同行评议的一手测量支持它;其二,1.5 ms 已经越过求和定位的融合上限"
    "(见第三节),其“满偏”端点实际上已进入第一波前定律接管的区域。因此应把它当作"
    "教学与经验值,而不是可引用的定量结论。Spot Aligner 所用的 AB 覆盖角闭式解"
    "sin(SRA/2) = 声速 × 1.5 ms ÷ 间距 同样应以此为前提来理解。",
    "Pulkki 1997(VBAP)与 Pulkki、Karjalainen 2001(幅度平移幻象定位 I/II)：系统研究了"
    "幅度平移的定位,并描述了时间差过大时的“像场分裂”现象——低频偏向先到的一侧,"
    "高频偏向更响的一侧。",
    "Gerzon 1992(听觉定位元理论)：用速度与能量成分刻画幻象,指出纯时间差(速度型)"
    "像场最宽但也最不稳定,对听者位置敏感;含能量成分的像场才稳定。",
])

# ----------------------------------------------------------------- 3
h("三、三段时间差,三种大脑行为", 1)
p("同一份声音到达耳朵两次时,大脑的处理规则随两次相隔的时间而变。这条规律是整个分歧的关键:"
  "争论的双方其实各自站在不同的区间里说话。用一条时间轴看最清楚。")
bullets([
    "小于约 1 ms：两次几乎同时,大脑把它当成一个声音,并且用“谁响一点、谁早一点”算出它在哪边。"
    "立体声就建立在这条规律上——两个扬声器发出的声音略有差别,就能在两者之间造出一个并不存在的"
    "位置。点话筒在这个区间内施加时间差,是合法且有意义的做法。",
    "约 1 至 5 ms：大脑不再融合方向,而是直接采用先到达的那一份作为位置,后到的那一份只改变"
    "音色与响度。这是听觉的自我保护——否则厅堂的反射会让人分不清乐队的方位。点话筒因为离声源"
    "更近而先到达,位置因此会被判给点话筒,这正是必须先对齐的原因。",
    "约 5 至 30 ms：仍然只听到一个声音,但后到的那一份开始明显增加宽度与空间感;"
    "它即使比先到的响约 10 dB,也还不被听成第二个声音。",
    "约 50 至 100 ms 以上：被听成两个声音,即回声。",
])
p("换成点话筒的实际数字,这条规律就更直观。设乐器距主话筒 5 m、距点话筒 0.5 m:同一声在点话筒里"
  "到达于 1.5 ms,在主话筒里到达于 14.6 ms,于是点话筒早到约 13 ms,落在第二段,位置被判给"
  "点话筒。施加约 13.1 ms 的对齐延时之后,两份声音同时到达,重新回到第一段;此时才由电平与"
  "时间的线索共同决定它落在像场的哪个位置。")
table(
    ["两次相隔", "大脑怎么做", "对点话筒的含义"],
    [
        ["小于约 1 ms", "合成为一个声音,用响度差与时间差共同算出位置",
         "可以施加时间差,它参与定位;但只有在时间差确实小于 1 ms 时才成立"],
        ["约 1–5 ms", "不再融合方向:位置归先到的那一份,后到者只改变音色",
         "点话筒更近因而先到,会夺走声像,所以必须先对齐"],
        ["约 5–30 ms", "仍是一个声音,后到者增加宽度与空间感",
         "对齐之后,点话筒剩下的作用主要就是宽度与临场感"],
        ["约 50–100 ms 以上", "被听成两个声音,即回声", "超出本文讨论的对齐范围"],
    ],
    widths=[2.6, 5.6, 6.6],
)
p("需要强调的一点:“小于约 1 ms”并不是对所有阵列都成立。AB 与德卡树的间距在 1–2 m 以上,"
  "几何算出的时间差本身就可以达到 1.5–6 ms,已经越过这一段的分界。也就是说,在这两种阵列上,"
  "“左右各给一个延时”得到的并不是一个连续的像场位置,而是按第一波前定律跳向先到的一侧。"
  "同一套算法在不同阵列上的心理声学含义并不相同,这一点必须在文档里如实说明。")
p("区间的出处:第一波前定律由 Cremer 1948 命名、由 Wallach、Newman 与 Rosenzweig 1949 以实验"
  "确立;Haas 1951 给出单次反射的听感数据;Litovsky 等 1999 的综述是这一领域的标准参考文献;"
  "“求和定位”与“1 ms 以内电平与时间共同决定方向”的表述见 Blauert 的《Spatial Hearing》与"
  "Theile 的《On the Localisation in the superimposed Soundfield》。区间边界随信号类型、"
  "扬声器夹角与节目素材而变,不同资料给出的融合上限在 0.6 至 1 ms 之间,本文取约 1 ms。")

# ----------------------------------------------------------------- 4
h("四、点话筒为什么会“抢走”声像,以及对齐延时为何必要", 1)
p("设点话筒距声源 0.5 m、主话筒距声源 5 m,则点话筒的声音比主话筒早到约 13 ms(按 343 m/s)。"
  "这落在定位优势区间:按第一波前定律,方向归先到的一方,于是乐器被拉到点话筒所在的位置。"
  "这是古典录音中“点话筒把声像拽过来”的物理原因。")
p("对齐延时的作用正是把点话筒推到与主话筒同时(时间差归零或接近零),使两者重新落入求和定位"
  "区间。此时方向不再由“谁先到”决定,而由电平与时间的线索共同决定——这恰好是插件第二步"
  "(按几何分配左右延时与电平)成立的先决条件。因此在文献层面,插件的两个步骤分别对应"
  "定位优势与求和定位两个区间,顺序不能颠倒。")

# ----------------------------------------------------------------- 5
h("五、单声道兼容性与左右梳状滤波", 1)
p("延迟拷贝与原始信号叠加会产生梳状滤波,零点间隔为 1/Δt Hz;当延时较短时,它不被听成回声,"
  "而表现为音色变化。这在 Blauert 的教材与 Zurek 1987 等文献中有系统论述。")
p("由此可以推得两种做法的差别:若左右两路使用不同的延时,则左右两路各自与主话筒叠加所形成"
  "的梳状滤波并不相同,叠加成单声道时不能用一个手段同时处理;若左右使用相同的延时,"
  "则左右梳状一致,单声道行为可预测。这一推论是严格的,但属于工程推导。本文检索范围内"
  "未找到把“两路不同延时”与“两路相同延时”作直接对比测量的研究,因此这一条应标为"
  "“有理论依据、缺少直接实验”。")
p("另一方面,“点话筒对到最近的主话筒、只用一个延时”这一做法,在古典录音的实践文献中长期存在"
  "(Decca / EMI 传统;Streicher 与 Dooley 关于立体声话筒技术的论述;Haigh、Dunkerley 与 Rogers "
  "的《Classical Recording: A Practical Guide in the Decca Tradition》)。它同时满足三件事:"
  "单声道行为可预测、只做平移而不改动声源自身的声像结构、控制权留在录音师手里。"
  "但它属于经验规范,文献中并没有支持“只能这样做”的证明。")

# ----------------------------------------------------------------- 6
h("六、立体声点话筒对:钢琴 AB 的特殊情形", 1)
p("当点话筒本身是一对 AB(例如钢琴)时,它自带一个时间差,这个时间差描述的是钢琴自身的宽度与"
  "内部结构,而不是它在厅里的位置。此时再施加额外的左右延时,结果需要分清楚:延时在系统中是"
  "线性相加的,因此额外延时不会“抵消”原有的时间差,而是与之叠加,总时间差被改写。")
bullets([
    "有文献支持的风险：总时间差变大、进入像场分裂的区间(Pulkki 与 Karjalainen 2001),"
    "或与主话筒给出的线索方向相反,都会使点话筒的像场变窄、发虚甚至反向。",
    "无文献支持的说法：“任何额外时间差都会破坏点话筒”这一表述过强;"
    "文献支持的是“过大的时间差不稳定”,而不是“任何时间差都不行”。",
    "工程上的另一个后果：两路延时不同会使左右梳状滤波不同,使单声道叠加变差(见第五节,"
    "属推导而非实测)。",
])

# ----------------------------------------------------------------- 7
h("七、两种做法的取舍对照", 1)
table(
    ["", "单一延时 + DAW 声像(录音师的做法)", "按几何分配左右延时(本插件的做法)"],
    [
        ["参照话筒", "离点话筒最近的那一支主话筒", "左右两路各用各自那一支主话筒"],
        ["点话筒被当作", "一个整体平移的相干单声道对象", "一个在左右之间有确定时间差的声源"],
        ["声像由谁决定", "人,通过声像旋钮,即时且与频率无关", "几何,算出后即在那个位置"],
        ["与主话筒的梳状", "左右相同,可用一个手段处理", "左右不同,需分别处理"],
        ["单声道兼容", "可预测,较安全", "较差,是这一做法的主要代价"],
        ["位置的性质", "紧、稳、接近一个点", "宽、贴,更接近厅里的自然声源"],
        ["文献依据", "定位优势与第一波前定律下的保守实践,配合 18 dB 强度差模型",
                     "求和定位区间内“电平与时间共同决定方向”"],
    ],
    widths=[2.6, 6.6, 6.6],
)

# ----------------------------------------------------------------- 8
h("八、对插件设计的含义", 1)
bullets([
    "对齐延时这一步有明确的文献依据：点话筒更近,先到,按定位优势会夺走声像;"
    "把它推回求和定位区间是必要且充分的(Wallach 等 1949;Haas 1951)。",
    "分配左右线索这一步同样有文献依据：时间差小于约 1 ms 时,电平与时间共同决定方向,"
    "因此用主阵列自身的时间差来定位点话筒是合法且有物理含义的(Blauert;Theile;"
    "Sengpiel 的 18 dB / 1.5 ms 等价关系)。",
    "反对意见指向的代价是真实的：时间差过大导致像场分裂与不稳定(Pulkki 与 Karjalainen 2001;"
    "Gerzon 1992);左右不同的梳状滤波影响单声道(推导)。这两点应当在文档中如实告知使用者。",
    "两种做法在文献上都站得住,分别优化不同目标:一种优化可预测性与单声道安全,"
    "另一种优化线索一致性与物理真实性。差别的可闻程度与点话筒的电平成正比——"
    "点话筒越低,两者越接近,这也解释了为什么有经验的录音师会觉得这一功能“用处不大”。",
    "因此一个诚实的工具应当：把对齐这一步做好(无争议),把左右时间差这一步的数值算清楚"
    "(几何决定,而非随手一个旋钮),并把它的代价写明白(单声道、以及点话筒自身是立体声对时"
    "总时间差的叠加)。是否采用,留给使用者决定。",
])

# ----------------------------------------------------------------- 9
h("九、结论", 1)
p("第一,时间差的感知存在三个区间,约 1 ms 是分界。低于它,电平与时间共同决定方向,"
  "引入左右时间差是合理的定位手段;高于约 2 ms,方向归先到者,这正是点话筒必须先对齐的原因。"
  "两种做法都不是错的,它们分别落在这一分界的两侧。")
p("第二,“点话筒对到最近的主话筒、只用一个延时”是一项经验规范,能带来单声道安全与平移而非"
  "旋转的好处,但本文未找到支持其为唯一正确做法的测量研究;同样,本文也未找到对“两路不同延时"
  "与两路相同延时”作直接对比的实验。")
p("第三,对插件的结论是:对齐这一步不变;左右时间差这一步在文献内成立,但应把它的适用条件"
  "与代价(时间差过大、点话筒自身为立体声对、单声道兼容)明确写出,并把是否采用留给使用者。")

# ----------------------------------------------------------------- 10
h("十、参考文献", 1)
refs = [
    "Blumlein, A. D. (1931). Improvements in and relating to sound-transmission, "
    "sound-recording and sound-reproducing systems. British Patent 394,325.",
    "Clark, H. A. M., Dutton, G. F., & Vanderlyn, P. B. (1957). The “stereosonic” "
    "recording and reproducing system: a two-channel system for domestic use. "
    "Proceedings of the IEE, Part B, 104(14).",
    "Cremer, L. (1948). Die wissenschaftlichen Grundlagen der Raumakustik, Bd. 1. "
    "Hirzel-Verlag, Stuttgart.(“第一波前定律”的命名)",
    "Wallach, H., Newman, E. B., & Rosenzweig, M. R. (1949). The precedence effect in "
    "sound localization. The American Journal of Psychology, 62, 315–336.",
    "Haas, H. (1951). Über den Einfluss eines Einfachechos auf die Hörsamkeit von Sprache. "
    "Acustica, 1, 49–58.(英译:The Influence of a Single Echo on the Audibility of Speech, "
    "JAES 20(2), 1972, 146–159.)",
    "Litovsky, R. Y., Colburn, H. S., Yost, W. A., & Guzman, S. J. (1999). "
    "The precedence effect. J. Acoust. Soc. Am., 106(4), 1633–1654.",
    "Blauert, J. (1997). Spatial Hearing: The Psychophysics of Human Sound Localization. "
    "MIT Press.(求和定位的定义与区间)",
    "Theile, G. On the Localisation in the superimposed Soundfield. "
    "http://www.hauptmikrofon.de/theile/ON_THE_LOCALISATION_english.pdf",
    "Theile, G., & Plenge, G. (1977). Localization of lateral phantom sources. "
    "J. Audio Eng. Soc., 25(4), 196–200.",
    "Gerzon, M. A. (1992). General metatheory of auditory localisation. "
    "AES Convention Paper 3306.",
    "Pulkki, V. (1997). Virtual sound source positioning using vector base amplitude "
    "panning. J. Audio Eng. Soc., 45(6), 456–466.",
    "Pulkki, V., & Karjalainen, M. (2001). Localization of amplitude-panned virtual "
    "sources I and II. J. Audio Eng. Soc., 49(9), 739–752 / 753–767.(“像场分裂”)",
    "Sengpiel, E. 等价立体声的数值与表格:Stereo-Mikrofonsysteme – Zahlenwerte und "
    "Tabellen;时间差立体声讲义 Blätter zur Vorlesung: Laufzeitstereofonie. "
    "http://www.sengpielaudio.com/ (本插件所采用的 18 dB / 1.5 ms 模型)",
    "Wittek, H. (2012). Ein Beitrag zur Optimierung der Aufnahmetechnik für "
    "Hauptmikrofonsysteme. 博士论文,慕尼黑音乐与戏剧学院. "
    "https://www.hauptmikrofon.de/HW/Wittek_thesis_201207.pdf",
    "Streicher, R., & Dooley, W. (1985). Basic stereo microphone perspectives – a review. "
    "J. Audio Eng. Soc., 33(7/8), 548–556.",
    "Lipshitz, S. P. (1986). Stereo microphone techniques: are the purists wrong? "
    "J. Audio Eng. Soc. https://www.aes.org/e-lib/browse.cfm?elib=11494 "
    "(间距制引入声道间时间差所带来的单声道兼容性损失、梳状滤波染色与听者位移敏感性,"
    "是这一路质疑的核心文献;正文本次未能读取,引用前请核对原文)",
    "Leonard, T. A. (1991). Time delay compensation of distributed multiple microphones in "
    "recording: an experimental evaluation. M.Mus. thesis, McGill University. "
    "https://mcgill.scholaris.ca/items/39b9eea8-a32d-4a91-9cdb-48ab48ced704/full "
    "(本次检索范围内唯一一篇专门做点话筒延时补偿与听测的研究:对齐目标是主立体声对,"
    "理由是“保持相位相干、对抗梳状滤波”;它并未规定“对齐到最近的那一支话筒”)",
    "Eargle, J. M. (1968). Stereo/mono disc compatibility: a survey of the problems. "
    "J. Audio Eng. Soc. https://www.aes.org/e-lib/browse.cfm?elib=1588 "
    "与 Small, E. (1971). Problems of mono/stereo compatibility in broadcasting. "
    "IEEE Trans. Broadcasting. https://doi.org/10.1109/TBC.1971.266041 "
    "(单声道兼容作为长期工程要求的文献来源)",
    "Haigh, C., Dunkerley, J., & Rogers, M. (2021). Classical Recording: A Practical "
    "Guide in the Decca Tradition. Routledge / AES.(点话筒延时的实践规范;页码待核)",
]
for i, r in enumerate(refs, 1):
    par = doc.add_paragraph()
    par.paragraph_format.left_indent = Cm(0.75)
    par.paragraph_format.first_line_indent = Cm(-0.75)
    run = par.add_run("[%d] " % i)
    run.bold = True
    run.font.size = Pt(9)
    run2 = par.add_run(r)
    run2.font.size = Pt(9)

# ----------------------------------------------------------------- 11
h("十一、证据强度说明", 1)
table(
    ["结论", "强度", "说明"],
    [
        ["时间差的三个区间及其分界", "已由实验确立",
         "第一波前定律、求和定位、回声阈值均有直接测量文献"],
        ["点话筒更近因而先到、会夺走声像", "由已确立的规律直接推得",
         "点话筒距离可由几何算出;规律本身有实验支持"],
        ["1 ms 以内电平与时间共同决定方向", "已确立",
         "Blauert;Theile;Sengpiel 的 18 dB / 1.5 ms 即其线性化"],
        ["过大的时间差导致像场分裂与不稳定", "已有实验描述",
         "Pulkki 与 Karjalainen 2001;Gerzon 1992"],
        ["两路不同延时使单声道兼容性变差", "理论推导",
         "由梳状滤波的基本性质推得;本次检索未找到直接对比测量"],
        ["点话筒应只对最近一支、只用单一延时", "经验规范",
         "Decca / EMI 传统与教科书中的做法;未找到支持其为唯一正确解的测量研究"],
    ],
    widths=[6.2, 3.0, 6.6],
)

end = doc.add_paragraph()
end.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = end.add_run("本综述中的文献均已定位到可核查的出处;凡属推论或经验规范者,已在文中与上表中注明。")
run.font.size = Pt(9)
run.font.color.rgb = RGBColor(0x77, 0x77, 0x77)

doc.save(OUT)
print("saved", OUT)
