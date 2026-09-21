# Corpus sintético de mecatrônica

QA-05 parcial. Casos definidos em tests/corpus_tests.cpp, sem arquivos privados.
Os arquivos .mcad/STEP são produzidos em diretório temporário e removidos pelo
próprio teste. Não são pacotes de aplicativo nem versões de distribuição.

| Caso | Geometria em mm | Volume esperado em mm³ | Cobertura |
| --- | --- | --- | --- |
| Suporte | Base 80×50×5; flange 80×5×40; dois furos Ø6 na base | 36000 − 90π | União, dois cortes, histórico |
| Caixa de sensor | Exterior 60×40×25; cavidade 56×36×23 | 13632 | Expressões, reconstrução, mudança de largura para 70 |
| Espaçador | Cilindro Ø24×10; furo passante Ø10 | 1190π | Corte cilíndrico e edição inválida atômica |

Cada caso verifica sólido válido, um corpo final, volume analítico, reconstrução
idempotente, salvar/reabrir em .mcad e exportar/reimportar STEP. Tolerância de
volume: 1e-5 mm³ no nativo e 1e-4 mm³ no STEP. Intercâmbio usa o próprio OCCT:
isso NÃO é validação por ferramenta independente nem fecha FAB-01.

Execução sem janelas: `sh scripts/check.sh corpus`.

Pendências: sketches compostos, superfícies, montagens, chapas, diversidade de
topologias, cargas grandes, máquinas/CAM e referências independentes. Não há
qualificação de fabricação nem ensaio físico dessas peças. A caixa varia apenas
a largura; a espessura não é um modelo de casca paramétrica completo.
