# -*- coding: utf-8 -*-
"""Writes the five packaged READMEs in the 简介 / 原理 / 要求 / 用法 structure."""
import os

OUT = r"D:\VST3 Programming\SpotAligner\dist_readmes"
os.makedirs(OUT, exist_ok=True)

docs = {}

docs["README-Chinese.txt"] = """Spot Aligner 1.0.0 — Windows x64 VST3

本文件夹内还有:README-English.txt、README-German.txt、README-French.txt、README-Japanese.txt。

简介
古典录音里,主话筒对负责整体,点话筒负责清晰度与临场感。把点话筒加进来时通常只有两个旋钮可用:
电平和声像。可一支点话筒不是一个声像旋钮 —— 它在房间里本来就有确定的位置:离主话筒多远、在哪个角度上。

Spot Aligner 把这个位置算出来并施加于信号:按声源到各支主话筒的距离与声速算出对齐延时;
按主话筒制式(AB / ORTF / XY / NOS / DIN / Decca Tree)算出左右两路各自应收到的延时与电平。
面板同时显示阵列覆盖角、左右延时与电平,以及阵列是否已偏离该制式的标准几何。

原理
① 为什么必须先对齐:点话筒离乐器近、主话筒远,点话筒信号先天早到;两者间隔超过约 1 毫秒时,
   方向由先到达的一方决定,乐器的声像会被点话筒拽过去。对齐延时把它推到与主话筒同时到达,
   方向仲裁权回到主话筒。
② 为什么左右要用不同的延时与电平:制式决定成像方式 —— AB 与德卡树靠时间差,XY 靠电平差,
   ORTF / NOS / DIN 两者兼有。点话筒要落进同一个像场,就必须复制同一套线索。
③ 德卡树的中置话筒真的参与运算:它有自己的延时线,等量送入左右,并因此稀释左右线索。
④ 两条边界如实说明:点话筒靠近声源时乐器不能近似为点声源,几何给出的对齐值只是代表性近似;
   左右只要存在时间差 Δt,这一轨在单声道折叠下就有梳状零点 f = 1/(2Δt) —— 这是时间差成像的
   固有代价,AB 主话筒对本身也有。

要求
★ 必须插在立体声轨道上 ★ 插件承载"一个单声道对象"(一支点话筒)并把它摆进立体声像场,
两个声道都会用到:输入两路先相加成这一个对象,再按几何分配左右。轨道给不出两个声道时,
插件不做任何处理(对象原样通过)并在面板提示;显示 "Track output: 2 channels" 就说明轨道是对的。
Windows 10 或 11 64 位;任何 VST3 宿主(REAPER、Nuendo、Cubase、Studio One、Bitwig、WaveLab 等)皆可。
Pro Tools 只加载 AAX 且拒绝未签名的 AAX,请用包装宿主(Blue Cat PatchWork、DDMF Metaplugin,
或免费的 Modalics Plugin Buddy)在其中运行本插件。macOS / AU 版本在计划中。
未做代码签名,首次扫描时 Windows 可能提示一次:选"更多信息"→"仍要运行"。

用法
安装:双击 Install.bat,或把 SpotAligner.vst3 文件夹复制到:
    C:\\Program Files\\Common Files\\VST3\\        所有用户(需管理员)
    %LOCALAPPDATA%\\Programs\\Common\\VST3\\       仅当前用户
然后让宿主重新扫描插件;列表里为 Spot Aligner,厂商 Forrest's Plug-ins。

基本用法:
  1. 只给点话筒轨道加这个插件,不要加在主话筒上。
  2. 填 Distance / Angle / Height —— 声源相对于主话筒阵列的距离、水平角度与高度。
  3. 选 Format(主话筒制式),插件载入该制式的间距、张开角与指向性;这三项随后都可改,
     改离标准值后面板会标出 edited。
  4. DAW 的声像旋钮请留在中间 —— 插件已按几何把它摆在应在的位置,再拧一次就是摆位两次。
  5. 建议打开宿主的单声道监听检查:左右有时间差 Δt 时会有梳状零点 f = 1/(2Δt),
     这是时间差成像的固有性质,用它判断点话筒电平是否给多了。

点话筒本身是一对话筒时(例如钢琴的 AB 对):
  · 两支话筒各自一条轨道、各挂一个插件实例,参数填成完全相同,动点(角度)取这一对的中点;
  · 这样插件只做"把整对有宽度的声源平移到中点位置",这一对自己的时间差完全不被触碰 ——
    宽度由这对话筒自己提供,位置由插件提供;
  · 两轨都要让插件看到立体声总线,否则插件会判定为单声道轨道而直通。

卸载:删掉 SpotAligner.vst3 文件夹即可。设置随工程保存,旧版本工程照常打开。

--
使用 VST 3 SDK 3.8.1 构建。VST 是 Steinberg Media Technologies GmbH 的商标。
感谢 VST 让每个人都能做插件。使用了 REAPER 作为主测试 DAW —— 不得不说,REAPER 的超快启动
比 Nuendo 用来测试好多了。

反馈 / Feedback:qiuzhenyan4135@163.com
作者:南开大学物理科学学院  Zhenyan Qiu
"""

docs["README-English.txt"] = """Spot Aligner 1.0.0 - Windows x64 VST3

Other languages in this folder: README-Chinese.txt, README-German.txt,
README-French.txt, README-Japanese.txt.

Introduction
In classical recording the main pair carries the whole picture while spot microphones add definition
and presence. When a spot is brought in there are usually only two knobs: level and pan. But a spot
microphone is not a pan knob - it already has a definite place in the room: a distance from the main
array and an angle.

Spot Aligner works that place out and applies it to the signal: an alignment delay from the distance
to each main microphone and the speed of sound, and, from the technique of the array (AB / ORTF / XY
/ NOS / DIN / Decca Tree), the delay and the level each side should receive. The panel shows the
recording angle of the array, both delays and both levels, and marks the array as edited once it no
longer matches the geometry of its technique.

Principle
1. Why the alignment comes first: the spot is close to the instrument and the main pair is far away,
   so the spot arrives early. Once two copies are more than about 1 ms apart, the direction is
   decided by whichever arrived first, and the spot pulls the image. The alignment delay puts it
   level with the main pair and hands the decision back to them.
2. Why the two sides get different delays and levels: the technique decides how the pair images -
   AB and Decca Tree by time difference, XY by level difference, ORTF / NOS / DIN by both. A spot
   that is to sit in the same image has to carry the same cues.
3. A Decca Tree really has its centre microphone in the audio path: its own delay line, fed equally
   to both sides, which is why it dilutes the outer pair's cues and widens the coverage.
4. Two boundaries stated honestly: close to the source the instrument cannot be treated as a point,
   so a geometry-derived alignment is a representative approximation; and any inter-channel time
   difference Δt gives this track comb nulls in mono at f = 1/(2Δt) - the inherent price of
   time-difference imaging, which an AB main pair pays as well.

Requirements
* IT MUST GO ON A STEREO TRACK. The plug-in carries one mono object - a spot microphone - and places
  it in the stereo image, so both channels are used: the two input channels are summed into that one
  object, which is then distributed by the geometry. When the track cannot carry two channels
  nothing is processed (the object passes through) and the panel says so; "Track output: 2 channels"
  means the track is right.
* Windows 10 or 11, 64 bit, and any VST3 host (REAPER, Nuendo, Cubase, Studio One, Bitwig, WaveLab).
* Pro Tools loads AAX only and refuses unsigned AAX; use a wrapper host (Blue Cat PatchWork, DDMF
  Metaplugin, or the free Modalics Plugin Buddy) to run this VST3 inside it. A macOS / AU build is
  planned.
* The files are not code signed, so Windows may ask once when the plug-in is first scanned: choose
  "More information" and "Run anyway".

How to use it
Install: run Install.bat, or copy the SpotAligner.vst3 folder into
    C:\\Program Files\\Common Files\\VST3\\        all users, administrator
    %LOCALAPPDATA%\\Programs\\Common\\VST3\\       this user only
Then let the host scan for plug-ins. It appears as Spot Aligner by Forrest's Plug-ins.

1. Put it on the spot track only, never on the main pair.
2. Enter Distance / Angle / Height - where the source sits relative to the main array.
3. Choose Format; the plug-in loads that technique's spacing, aiming and polar pattern. All three
   stay editable, and the panel marks the array edited once they leave the standard values.
4. LEAVE THE HOST'S PAN CENTRED. The plug-in has already placed the spot by geometry; panning it
   again places it twice.
5. Check in mono. With an inter-channel time difference Δt there are comb nulls at f = 1/(2Δt);
   that is inherent to time-difference imaging, and it is a good way to judge whether the spot is
   set too loud.

When the spot is itself a pair (a piano AB pair, for instance):
  * one track and one plug-in instance per microphone, with IDENTICAL settings, and the point (the
    angle) taken at the midpoint of the two;
  * the plug-in then only shifts a source that has width to the position of that midpoint, and the
    pair's own time difference is not touched at all - the width comes from the pair, the position
    from the plug-in;
  * both tracks must let the plug-in see a stereo bus, otherwise it treats them as mono and passes
    through.

Uninstall: delete the SpotAligner.vst3 folder. Settings are stored with the project and older
projects keep working.

--
Built with the VST 3 SDK 3.8.1. VST is a trademark of Steinberg Media Technologies GmbH.
Thanks to VST for letting anybody make a plug-in. Tested mainly in REAPER - whose instant start-up
beats using Nuendo for the job.

Feedback: qiuzhenyan4135@163.com
Author: Zhenyan Qiu, School of Physics, Nankai University
"""

docs["README-German.txt"] = """Spot Aligner 1.0.0 - Windows x64 VST3

Weitere Sprachen in diesem Ordner: README-Chinese.txt, README-English.txt,
README-French.txt, README-Japanese.txt.

Einfuehrung
In der klassischen Aufnahme traegt das Hauptmikrofon das Gesamtbild, waehrend Stuetzmikrofone
Durchzeichnung und Praesenz beisteuern. Beim Zumischen gibt es meist nur zwei Regler: Pegel und
Panorama. Ein Stuetzmikrofon ist aber kein Panoramaregler - es hat im Raum bereits einen bestimmten
Platz: einen Abstand zum Hauptmikrofon-Array und einen Winkel.

Spot Aligner berechnet diesen Platz und wendet ihn auf das Signal an: eine Ausrichtungsverzoegerung
aus dem Abstand zu jedem Hauptmikrofon und der Schallgeschwindigkeit, und aus dem Verfahren des
Arrays (AB / ORTF / XY / NOS / DIN / Decca Tree) die Verzoegerung und den Pegel, die jede Seite
erhalten soll. Das Fenster zeigt den Aufnahmewinkel, beide Verzoegerungen und beide Pegel und meldet
"edited", sobald das Array von der Geometrie seines Verfahrens abweicht.

Prinzip
1. Warum zuerst ausgerichtet wird: das Stuetzmikrofon steht nah am Instrument, das Hauptmikrofon
   weit weg, also kommt es zu frueh. Liegen zwei Kopien mehr als etwa 1 ms auseinander, entscheidet
   die zuerst eintreffende ueber die Richtung, und das Stuetzmikrofon zieht das Klangbild zu sich.
   Die Ausrichtungsverzoegerung bringt es auf gleiche Zeit und gibt die Entscheidung zurueck.
2. Warum beide Seiten unterschiedliche Verzoegerungen und Pegel bekommen: das Verfahren bestimmt die
   Abbildung - AB und Decca Tree ueber die Laufzeitdifferenz, XY ueber den Pegelunterschied,
   ORTF / NOS / DIN ueber beides. Ein Stuetzmikrofon, das im selben Klangbild sitzen soll, muss
   dieselben Merkmale tragen.
3. Ein Decca Tree hat sein Mittelmikrofon wirklich im Signalweg: eigene Verzoegerungsleitung, gleich
   auf beide Seiten - deshalb verduennt es die Merkmale des aeusseren Paares und erweitert die
   Abdeckung.
4. Zwei Grenzen, ehrlich gesagt: nahe an der Quelle kann das Instrument nicht als Punkt gelten, eine
   aus der Geometrie abgeleitete Ausrichtung ist daher eine repraesentative Naeherung; und jede
   Laufzeitdifferenz Δt erzeugt auf dieser Spur im Mono-Summen Nullstellen bei f = 1/(2Δt) - der
   inhärente Preis der Laufzeit-Stereofonie, den ein AB-Hauptmikrofon ebenfalls zahlt.

Voraussetzungen
* ES MUSS AUF EINE STEREOSPUR. Das Plug-in traegt ein einzelnes Mono-Objekt - ein Stuetzmikrofon -
   und setzt es ins Stereobild, deshalb werden beide Kanaele gebraucht: die zwei Eingangskanaele
   werden zu diesem einen Objekt summiert und danach nach der Geometrie verteilt. Kann die Spur
   keine zwei Kanaele liefern, wird nichts bearbeitet (das Objekt geht unveraendert durch) und das
   Fenster sagt es; "Track output: 2 channels" heisst, die Spur stimmt.
* Windows 10 oder 11, 64 Bit, und ein beliebiger VST3-Host (REAPER, Nuendo, Cubase, Studio One,
  Bitwig, WaveLab).
* Pro Tools laedt nur AAX und verweigert unsigniertes AAX; nutzen Sie einen Wrapper-Host (Blue Cat
  PatchWork, DDMF Metaplugin oder das kostenlose Modalics Plugin Buddy). Eine macOS-/AU-Version ist
  geplant.
* Die Dateien sind nicht signiert, Windows fragt daher beim ersten Einlesen eventuell einmal nach:
  "Weitere Informationen" und "Trotzdem ausfuehren".

Bedienung
Installation: Install.bat starten, oder den Ordner SpotAligner.vst3 kopieren nach
    C:\\Program Files\\Common Files\\VST3\\        alle Benutzer, Administrator
    %LOCALAPPDATA%\\Programs\\Common\\VST3\\       nur dieser Benutzer
Dann im Host die Plug-ins neu einlesen. Es erscheint als Spot Aligner von Forrest's Plug-ins.

1. Nur auf die Stuetzmikrofon-Spur legen, nie auf das Hauptmikrofon.
2. Distance / Angle / Height eingeben - die Lage der Quelle relativ zum Hauptmikrofon-Array.
3. Format waehlen; das Plug-in laedt Abstand, Winkel und Richtcharakteristik des Verfahrens. Alle
   drei bleiben aenderbar, und das Fenster meldet "edited", sobald sie vom Standard abweichen.
4. DEN PANORAMAREGLER DES HOSTS IN DER MITTE LASSEN. Das Plug-in hat das Stuetzmikrofon bereits
   nach der Geometrie platziert; erneutes Pannen platziert es zweimal.
5. In Mono pruefen. Bei einer Laufzeitdifferenz Δt entstehen Nullstellen bei f = 1/(2Δt); das ist
   der Laufzeit-Stereofonie eigen und ein gutes Mittel, um zu beurteilen, ob das Stuetzmikrofon zu
   laut ist.

Ist das Stuetzmikrofon selbst ein Paar (etwa ein AB-Paar am Klavier):
  * je Mikrofon eine Spur und eine Instanz, mit VOELLIG GLEICHEN Einstellungen, und der Punkt
    (der Winkel) in der Mitte der beiden;
  * das Plug-in verschiebt dann nur eine Quelle mit Breite an die Position dieser Mitte, und die
    eigene Laufzeitdifferenz des Paares bleibt unberuehrt - die Breite kommt vom Paar, die Position
    vom Plug-in;
  * beide Spuren muessen dem Plug-in einen Stereobus zeigen, sonst behandelt es sie als Mono und
    leitet unveraendert durch.

Deinstallation: den Ordner SpotAligner.vst3 loeschen. Einstellungen liegen im Projekt, aeltere
Projekte laufen weiter.

--
Gebaut mit dem VST 3 SDK 3.8.1. VST ist eine Marke der Steinberg Media Technologies GmbH.
Dank an VST dafuer, dass jeder ein Plug-in bauen kann. Hauptsaechlich in REAPER getestet - dessen
blitzschneller Start ist zum Testen angenehmer als Nuendo.

Feedback: qiuzhenyan4135@163.com
Autor: Zhenyan Qiu, Fakultaet fuer Physik, Nankai-Universitaet
"""

docs["README-French.txt"] = """Spot Aligner 1.0.0 - Windows x64 VST3

Autres langues dans ce dossier : README-Chinese.txt, README-English.txt,
README-German.txt, README-Japanese.txt.

Introduction
En prise de son classique, le couple principal porte l'image globale tandis que les micros d'appoint
apportent definition et presence. Pour les ajouter, on ne dispose guere que de deux reglages :
niveau et panoramique. Or un micro d'appoint n'est pas un potentiomètre de panoramique - il a deja
une place determinee dans la salle : une distance au couple principal et un angle.

Spot Aligner calcule cette place et l'applique au signal : un delai d'alignement a partir de la
distance a chaque micro principal et de la vitesse du son, et, d'apres la technique du couple
(AB / ORTF / XY / NOS / DIN / Decca Tree), le delai et le niveau que chaque cote doit recevoir. Le
panneau affiche l'angle de prise, les deux delais et les deux niveaux, et marque "edited" des que le
couple s'ecarte de la geometrie de sa technique.

Principe
1. Pourquoi l'alignement vient d'abord : le micro d'appoint est proche de l'instrument et le couple
   principal est loin, donc il arrive trop tot. Au-dela d'environ 1 ms d'ecart, la direction est
   decidee par ce qui arrive en premier, et l'appoint tire l'image a lui. Le delai d'alignement le
   remet a l'heure et rend la decision au couple principal.
2. Pourquoi les deux cotes recoivent des delais et des niveaux differents : la technique decide de
   l'image - AB et Decca Tree par difference de temps, XY par difference de niveau, ORTF / NOS / DIN
   par les deux. Un appoint qui doit tenir dans la meme image doit porter les memes indices.
3. Un Decca Tree a vraiment son micro central dans le chemin du signal : sa propre ligne de retard,
   envoyee egalement des deux cotes - d'ou la dilution des indices du couple externe et
   l'elargissement de la couverture.
4. Deux limites dites honnetement : pres de la source, l'instrument ne peut pas etre traite comme un
   point, donc un alignement deduit de la geometrie reste une approximation representative ; et tout
   ecart temporel Δt entre canaux donne a cette piste des zeros en mono a f = 1/(2Δt) - le prix
   inherent de la stereophonie par difference de temps, que paie aussi un couple AB.

Configuration requise
* A POSER SUR UNE PISTE STEREO. Le plug-in porte un seul objet mono - un micro d'appoint - et le
   place dans l'image stereo : les deux canaux servent donc. Les deux canaux d'entree sont d'abord
   additionnes en cet objet, qui est ensuite reparti par la geometrie. Si la piste ne peut pas
   fournir deux canaux, rien n'est traite (l'objet passe tel quel) et le panneau le signale ;
   "Track output: 2 channels" signifie que la piste est correcte.
* Windows 10 ou 11, 64 bits, et tout hote VST3 (REAPER, Nuendo, Cubase, Studio One, Bitwig, WaveLab).
* Pro Tools ne charge que l'AAX et refuse l'AAX non signe ; utilisez un hote d'accueil (Blue Cat
   PatchWork, DDMF Metaplugin, ou le gratuit Modalics Plugin Buddy). Une version macOS / AU est
   prevue.
* Les fichiers ne sont pas signes : Windows peut demander une fois au premier scan. Choisissez
   "Informations complementaires" puis "Executer".

Utilisation
Installation : lancez Install.bat, ou copiez le dossier SpotAligner.vst3 dans
    C:\\Program Files\\Common Files\\VST3\\        tous les utilisateurs, administrateur
    %LOCALAPPDATA%\\Programs\\Common\\VST3\\       cet utilisateur seulement
Puis faites rescanner les plug-ins par l'hote. Il apparait comme Spot Aligner par Forrest's Plug-ins.

1. Ne le mettez que sur la piste du micro d'appoint, jamais sur le couple principal.
2. Entrez Distance / Angle / Height - la position de la source par rapport au couple principal.
3. Choisissez Format ; le plug-in charge l'ecartement, l'angle et la directivite de la technique.
   Les trois restent modifiables, et le panneau marque "edited" des qu'ils s'ecartent du standard.
4. LAISSEZ LE PANORAMIQUE DE L'HOTE AU CENTRE. Le plug-in a deja place l'appoint par la geometrie ;
   le panner a nouveau revient a le placer deux fois.
5. Verifiez en mono. Avec un ecart temporel Δt, il y a des zeros a f = 1/(2Δt) ; c'est inherent a la
   stereophonie par difference de temps, et c'est un bon moyen de juger si l'appoint est trop fort.

Quand l'appoint est lui-meme une paire (un couple AB au piano, par exemple) :
  * une piste et une instance par micro, avec des REGLAGES IDENTIQUES, et le point (l'angle) pris au
    milieu des deux ;
  * le plug-in ne fait alors que deplacer une source qui a de la largeur vers la position de ce
    milieu, et la difference de temps propre a la paire n'est pas touchee du tout - la largeur vient
    de la paire, la position du plug-in ;
  * les deux pistes doivent laisser le plug-in voir un bus stereo, sinon il les traite comme mono et
    laisse passer.

Desinstallation : supprimez le dossier SpotAligner.vst3. Les reglages sont enregistres avec le
projet et les anciens projets continuent de fonctionner.

--
Compile avec le VST 3 SDK 3.8.1. VST est une marque de Steinberg Media Technologies GmbH.
Merci a VST de permettre a chacun de faire un plug-in. Teste principalement dans REAPER - dont le
demarrage instantane est plus agreable que Nuendo pour ce travail.

Feedback : qiuzhenyan4135@163.com
Auteur : Zhenyan Qiu, Faculte de physique, Universite de Nankai
"""

docs["README-Japanese.txt"] = """Spot Aligner 1.0.0 - Windows x64 VST3

同じフォルダ内の他の言語:README-Chinese.txt、README-English.txt、README-German.txt、
README-French.txt。

概要
クラシック録音では、メインアレイが全体像を担い、スポットマイクが明瞭さと臨場感を加えます。
スポットを混ぜるときに使えるつまみは通常ふたつ、レベルとパンだけです。しかしスポットマイクは
パンつまみではありません —— 部屋の中にすでに決まった位置があります:メインアレイからの距離と
角度です。

Spot Aligner はその位置を計算して信号に与えます:各メインマイクまでの距離と音速から求める整列用
の遅延と、アレイの方式(AB / ORTF / XY / NOS / DIN / Decca Tree)に応じた左右それぞれの遅延と
レベルです。パネルにはアレイの収録角度、左右の遅延とレベル、そしてアレイが方式の標準形状から
外れたかどうかが表示されます。

原理
1. なぜ先に整列するのか:スポットは楽器に近く、メインは遠いため、スポットは必ず早く届きます。
   ふたつのコピーの隔たりが約 1 ms を超えると、方向は先に届いた側で決まり、スポットが音像を
   引っ張ります。整列用の遅延で同時にそろえると、判断はメイン側に戻ります。
2. なぜ左右で異なる遅延とレベルを与えるのか:方式が結像の仕組みを決めます —— AB と Decca Tree は
   時間差、XY はレベル差、ORTF / NOS / DIN はその両方です。同じ音像に収まるスポットは、同じ
   手がかりを備えている必要があります。
3. Decca Tree のセンターマイクは実際に信号経路に入ります:独自の遅延線を持ち、左右へ等しく
   送られます —— そのため外側の手がかりを薄め、カバー角度を広げます。
4. 正直に記すふたつの限界:音源に近い位置では楽器を点とはみなせないため、幾何から求めた整列は
   代表的な近似にすぎません。また、左右に時間差 Δt があるかぎり、このトラックはモノラルで
   f = 1/(2Δt) にコム状の零点を持ちます —— これは時間差ステレオ固有の代償で、AB のメイン
   マイクも同じものを払っています。

動作環境
★ ステレオトラックに挿してください ★ このプラグインは「1つのモノラルオブジェクト」(スポット
マイク1本)を扱い、それをステレオの音像に配置するため、左右の両チャンネルを使います:入力の2ch は
いったんこの1つのオブジェクトに加算され、その後で幾何に従って左右へ配分されます。トラックが
2ch を出せない場合、何も処理されず(オブジェクトはそのまま通過)、パネルに表示されます。
"Track output: 2 channels" と出ていれば正しい状態です。
Windows 10 / 11、64ビット。VST3 対応ホスト(REAPER、Nuendo、Cubase、Studio One、Bitwig、
WaveLab など)。
Pro Tools は AAX のみを読み込み、署名のない AAX を拒否します。ラッパーホスト(Blue Cat
PatchWork、DDMF Metaplugin、または無料の Modalics Plugin Buddy)をご利用ください。
macOS / AU 版は計画中です。
コード署名をしていないため、初回スキャン時に Windows が一度確認を出すことがあります。
「詳細情報」→「実行」を選んでください。

使い方
インストール:Install.bat を実行するか、SpotAligner.vst3 フォルダを次へコピーしてください:
    C:\\Program Files\\Common Files\\VST3\\        全ユーザー(管理者権限)
    %LOCALAPPDATA%\\Programs\\Common\\VST3\\       現在のユーザーのみ
その後ホストで再スキャンしてください。Spot Aligner(ベンダー:Forrest's Plug-ins)として
表示されます。

1. スポットマイクのトラックにのみ挿してください。メインには挿さないでください。
2. Distance / Angle / Height を入力 —— メインアレイに対する音源の距離・水平角度・高さです。
3. Format を選ぶと、その方式の間隔・開き角・指向特性が読み込まれます。3つとも後から変更でき、
   標準値から外れるとパネルに edited と表示されます。
4. ホストのパンは中央のままにしてください —— プラグインが幾何に従ってすでに配置しています。
   もう一度パンを動かすと二重に配置することになります。
5. モノラルで確認してください。左右に時間差 Δt があると f = 1/(2Δt) にコム状の零点が出ます。
   これは時間差ステレオ固有の性質で、スポットのレベルが高すぎないかの判断に使えます。

スポット自体が一对(例:ピアノの AB 対)の場合:
  · マイクごとに1トラック・1インスタンスとし、設定を完全に同一にし、点(角度)は2本の
    中点に取ります。
  · そうするとプラグインは「幅を持った音源を中点の位置へ平行移動」するだけになり、その対自身の
    時間差には一切触れません —— 幅は対が与え、位置はプラグインが与えます。
  · 両トラックともプラグインにステレオバスを見せる必要があります。そうでなければモノラルと
    判定して素通りします。

アンインストール:SpotAligner.vst3 フォルダを削除するだけです。設定はプロジェクトと共に保存され、
旧バージョンのプロジェクトもそのまま開けます。

--
VST 3 SDK 3.8.1 でビルド。VST は Steinberg Media Technologies GmbH の商標です。
VST のおかげで誰でもプラグインを作れます。主に REAPER でテストしました —— その起動の速さは、
テストに Nuendo を使うより快適です。

フィードバック: qiuzhenyan4135@163.com
作者:南開大学物理科学学院  Zhenyan Qiu
"""

for name, text in docs.items():
    path = os.path.join(OUT, name)
    with open(path, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write(text)
    print(name, len(text), "chars")
print("written to", OUT)
