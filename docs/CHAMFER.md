# Chanfro — versão de desenvolvimento

Disponível em MODIFY → Chamfer. Selecione uma ou mais arestas de um único sólido
com o filtro de arestas e Shift. Não se aplica a malhas STL.

- Equal distance: distância igual nas duas faces adjacentes.
- Two distances: duas distâncias; Reference side troca a face da primeira medida.
- Distance and angle: distância e ângulo estritamente entre 0 e 90 graus.

A prévia acompanha as medidas. Aceitar adiciona um recurso; Cancel/Esc preservam
o documento. Medidas que não geram geometria válida mostram erro e impedem aceitar.
Selecione o chanfro no Browser/histórico e execute Chamfer novamente para editar
suas medidas/modo, sem criar outro corpo. Selecionar arestas do resultado permite
criar outro chanfro sobre ele. Distâncias podem usar vínculos de parâmetros.

Limites: seleção de arestas é anterior à abertura; ainda não há edição dinâmica
da seleção no painel ou manipulador por arraste. A referência usa índice de
aresta e guarda a quantidade de arestas da origem; isso rejeita algumas mudanças
de topologia, mas NÃO resolve renumeração mantendo a mesma contagem (PAR-01).
Troca de lado é visualizada pela prévia, sem realce específico da face de referência.
Validação inicial cobre sólidos prismáticos; outras topologias ainda precisam de
corpus ampliado. GEO-04 permanece parcial. Distribuição em dist não foi alterada.

Evidências: core chamferModesAndPersistence/chamferRejectsBadInputs; UI
chamferPreviewEditAndCancel; captura build/chamfer-preview.png. O teste calcula o
volume removido em cada modo, verifica edição, seleção, prévia, falha, undo/redo,
persistência e integridade BRep. O teste de filete é repetido por compartilhar UI.
