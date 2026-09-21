# MecaCAD 0.2 — interface e interação direta

## O que mudou

A referência visual usada foi o print escuro do Fusion enviado pelo usuário.
A barra superior agora é compacta, com ferramentas por ícones, grupos e menus.
O Browser fica sobre o canvas; navegação e histórico ocupam faixas pequenas.
O viewport tem fundo em gradiente, arestas topológicas e cubo de orientação clicável.
Ícones são desenhados pelo aplicativo, sem arquivos gráficos extraídos do Fusion.

No fluxo de modelagem, Create Sketch mostra planos clicáveis. Retângulo e círculo
continuam usando dois cliques. Extrusão tem seta arrastável e prévia. Move/Copy
tem setas de translação X/Y/Z, com os campos de precisão recolhidos inicialmente.
Os painéis não bloqueiam a navegação da câmera. Enter confirma a prévia e Escape
cancela. Selecionar uma peça não abre mais o formulário de propriedades.

## Verificação

- Testes do núcleo: geometria, histórico, recálculo, formatos e validações.
- Teste gráfico: escolher plano com mouse, desenhar retângulo, extrudar por arraste,
  verificar volume, desfazer/refazer, cancelar sem mudar o documento, mover por
  arraste, cancelar movimento e salvar/reabrir.
- Capturas reais do aplicativo e dos painéis de extrusão e movimento inspecionadas.

## Limites explícitos

Não é uma reprodução integral do Fusion. Ainda faltam restrições gerais de sketch,
cotas e vértices arrastáveis, seleção individual de arestas, anéis de rotação,
menu radial, rollback do histórico, múltiplos documentos simultâneos e montagens.
Furos, filetes, primitivas e rotação ainda dependem de campos numéricos.
O sketch por face aceita apenas alinhamento aos planos globais e grava o offset,
sem manter associação topológica com a face. A prévia recalcula sincronamente;
modelos grandes podem interromper a fluidez.

Simulação, CAM, desenhos técnicos e importação nativa do Fusion não estão incluídos.
Os menus de funções ainda não disponíveis ficam desabilitados.

## Pacote

`dist/MecaCAD-0.2.app` e `dist/MecaCAD-0.2-macOS-arm64.zip`.
A versão anterior é preservada. Os projetos `.mcad` mantêm o formato anterior;
o arquivo de exemplo editado pelo usuário não faz parte desta revisão de código.
