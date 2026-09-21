# Encaixe inteligente no sketch

O menu Grid and snaps → Encaixe inteligente ativa a atração a extremidades,
pontos médios, centros, quadrantes, cruzamentos e alinhamentos. O rótulo no
canvas identifica o tipo de ponto encontrado; as guias indicam alinhamento.

Interseções reconhecidas: linha/linha, linha/círculo, círculo/círculo e as mesmas
combinações com arcos circulares. Segmentos não são prolongados. Arcos aceitam
somente pontos do trecho desenhado, inclusive em sentido inverso e arcos maiores
que meia volta. Tangência produz um único ponto. Círculos coincidentes não geram
um ponto arbitrário de interseção.

O alcance é medido em pixels: aquisição a até 10 pixels e manutenção do alvo
até 15 pixels. A busca de círculos para interseção é limitada à vizinhança do
cursor. Geometria oculta, suprimida ou de outro plano não participa, salvo o
sketch selecionado conforme as regras de edição já existentes.

**Encaixe não cria restrição permanente.** Ele define a coordenada usada pelo
gesto. Para preservar uma relação ao editar medidas, use os comandos Constraints.
Desativar Encaixe inteligente não desativa o snap independente da grade de 1 mm.

## Testes e limites

- `sh scripts/check.sh snapping`: testes matemáticos sem janelas.
- UI `curvedIntersectionsSnapOnSketchPlanes`, `arcIntersectionDoesNotExtendArc`
  e `smartSketchSnapping`: integração, planos XY/XZ/YZ, zoom e regressão.
- Não há interseção de splines/elipses, projeção automática de arestas de sólidos
  nem inferência de tangência como restrição. Essas entidades/fluxos não foram
  declarados implementados por este incremento.

Mudanças em desenvolvimento; nenhuma nova distribuição é produzida pelos testes.
