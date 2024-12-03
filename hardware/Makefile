# Makefile for building board artifacts

.PHONY: all clean

KIKIT := kikit

BOARDS = main-board trans-led-board
PANELS = $(addsuffix -panel, $(BOARDS))
MINIPANELS = $(addsuffix -panel-mini, $(BOARDS))
BOARDFILES = $(foreach board,$(BOARDS),$(addsuffix .kicad_pcb, $(addprefix $(board)/,$(board))))
PANELFILES = $(addprefix build/, $(PANELS:=.kicad_pcb))
MINIPANELFILES = $(addprefix build/, $(MINIPANELS:=.kicad_pcb))
GERBERS = $(addprefix build/, $(PANELS:=-gerber.zip))

all: $(GERBERS)

panels: $(PANELFILES)

mini-panels: $(MINIPANELFILES)

build/main-board-panel.kicad_pcb: main-board/main-board.kicad_pcb main-board/main-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p main-board/main-board-panelize.json $< $@

build/main-board-panel-mini.kicad_pcb: main-board/main-board.kicad_pcb main-board/main-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p main-board/main-board-panelize.json \
		-l 'grid; rows: 2; cols: 1;' \
		-r 'railstb; width: 5mm; space: 3.25mm;' \
		-o '4hole; hoffset: 2.5mm; voffset: 2.5mm; size: 2.05mm;' \
		$< $@

build/trans-led-board-panel.kicad_pcb: trans-led-board/trans-led-board.kicad_pcb trans-led-board/trans-led-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p trans-led-board/trans-led-board-panelize.json $< $@

build/trans-led-board-panel-mini.kicad_pcb: trans-led-board/trans-led-board.kicad_pcb trans-led-board/trans-led-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p trans-led-board/trans-led-board-panelize.json \
		-l 'grid; rows: 1; cols: 5;' \
		-r 'railstb; width: 5mm; space: 6mm;' \
		-t 'text: Transmission LED Board Panel' \
		$< $@

%-gerber: %.kicad_pcb
	kikit export gerber $< $@

%-gerber.zip: %-gerber
	zip -j $@ `find $<`

build:
	mkdir -p build

clean:
	rm -rf build
