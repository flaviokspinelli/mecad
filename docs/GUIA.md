# Guia rápido — MecaCAD 0.1

## Primeira peça

1. Clique em **Create Sketch**, no canto esquerdo da barra de criação.
2. Escolha **XY — Top**, offset zero, e confirme.
3. A ferramenta de retângulo começa ativa. Clique em dois cantos na área de desenho.
4. No painel da direita, altere **Width**, **Height**, **X** e **Y**, e clique em **Apply**.
5. Clique em **Finish Sketch** no lado direito da barra.
6. Clique em **Extrude** ou pressione **E**. Escolha o sketch, a distância e **New Body**.
7. Salve com **File → Save**. No Mac, use **⌘S**.

Para criar por números sem desenhar primeiro, use **Create → Create by dimensions…**
dentro do ambiente Sketch. Cotas visuais aparecem no sketch selecionado durante
a edição. Na versão atual, a edição numérica acontece no painel direito.

## Onde estão os comandos

| Região | Função |
| --- | --- |
| File, na barra de menus do macOS | Abrir, salvar, importar STEP e exportar |
| Create | Sketch, Extrude, Revolve, Hole, Box, Cylinder, Sphere |
| Modify | Fillet, Move / Copy e Combine |
| Inspect | Measure: dimensões externas e volume |
| Browser, à esquerda | Sketches, corpos finais e operações anteriores |
| History, embaixo | Selecionar uma operação; duplo clique abre seus parâmetros |
| Painel direito | Editar dimensões e nome; Apply confirma |
| Botão direito no canvas | Menu contextual de ferramentas |
| Botão direito no sketch, no Browser | Edit Sketch, Extrude e DXF |

Assemble, Construct e demais funções planejadas são referências de organização e
estão desabilitadas. O nome **MecaCAD** identifica um aplicativo independente.

## Navegação e atalhos

| Ação | Controle |
| --- | --- |
| Selecionar corpo ou contorno de sketch | Clique esquerdo |
| Pan | Arrastar com botão central |
| Órbita | Shift + arrastar com botão central |
| Zoom | Roda do mouse |
| Enquadrar geometria visível | F |
| Trackpad: pan | Deslizar com dois dedos |
| Trackpad: órbita | Shift + deslizar com dois dedos |
| Trackpad: zoom | Pinça |
| Alternativa para órbita sem botão central | Option/Alt + arraste esquerdo |
| Alternativa para pan sem botão central | Shift + Option/Alt + arraste esquerdo |
| Line / Rectangle / Circle | L / R / C |
| Sketch Dimension | D |
| Extrude / Hole / Move / Measure | E / H / M / I |
| Busca de comandos | S |
| Salvar / desfazer / refazer no Mac | ⌘S / ⌘Z / ⇧⌘Z |
| Cancelar ferramenta ou perfil em andamento | Esc |
| Concluir polilinha aberta / fechada | Enter / Shift+Enter |

A câmera permanece perpendicular ao plano durante o sketch. O atalho de dimensão
leva ao painel de parâmetros do objeto selecionado. As combinações de pan e
órbita seguem a [referência de navegação da Autodesk](https://www.autodesk.com/products/fusion-360/blog/quick-tip-pan-zoom-orbit-preferences/).

## Sketches e planos

Cada retângulo, círculo, arco ou cadeia de linhas cria um sketch independente.
Uma polilinha fecha ao clicar novamente no primeiro ponto ou usar Shift+Enter.
Arcos usam três cliques: início, ponto intermediário e fim. Arcos e linhas abertas
podem ser exportados em DXF, mas não extrudados como sólidos.

O snap arredonda as coordenadas ao milímetro; desative **Snap 1 mm** para desenho
livre. Para precisão final, digite os valores no painel. A grade visual adapta o
espaçamento ao zoom e não altera a unidade dos parâmetros.

| Plano | U | V | Normal positiva / offset |
| --- | --- | --- | --- |
| XY | X | Y | +Z |
| XZ | X | Z | −Y |
| YZ | Y | Z | +X |

A extrusão positiva segue a normal do plano. Distância negativa inverte o sentido.
Para cortar uma peça, escolha o corpo de destino e **Cut**. A revolução acontece
em torno do eixo vertical local, na coordenada U informada.

## Furos e operações sólidas

**Hole** usa centro U/V, plano, offset inicial, raio e profundidade. Na base de
8 mm do exemplo, um furo em XY pode começar no offset −1 e ter profundidade 10.
Se o cilindro do furo não alcançar o corpo, a operação é rejeitada com uma mensagem.

**Combine** exige corpos de destino e ferramenta diferentes. Join une, Cut subtrai
e Intersect conserva a região comum. Os corpos usados aparecem nas operações
anteriores e o resultado passa a ser o corpo final. **Fillet** atua em todas as
arestas; diminua o raio se a geometria não comportar o valor escolhido.

Mover/copy usa rotação em torno da origem global, seguida da translação. A opção
**Create Copy** preserva o corpo original. A visibilidade é controlada no Browser.

## Editar, salvar e recuperar

Escolha o sketch ou operação no Browser/History, mude seus parâmetros e clique
em Apply. As operações seguintes são recalculadas. Uma edição inválida preserva
o documento anterior. Não é possível excluir uma operação ainda referenciada:
remova primeiro suas dependentes.

O projeto `.mcad` contém parâmetros e histórico. O aplicativo oferece salvar ao
fechar um documento alterado. A recuperação automática é gravada a cada 30 segundos
na pasta de dados do MecaCAD e oferecida na próxima abertura após interrupção.
Ela não substitui salvar regularmente; o intervalo mais recente pode não ter sido gravado.

## Exportar

- **STEP**: sólido selecionado; sem seleção de sólido, exporta os corpos finais visíveis.
- **STL**: mesma regra de seleção. É uma malha de triângulos, sem histórico. As
  coordenadas são em mm; confirme mm ao abrir no fatiador, pois STL não registra unidade.
- **DXF**: selecione um sketch no Browser. O arquivo contém seu desenho 2D nas
  coordenadas locais do plano, com indicação de milímetros.

O STEP importado é incorporado ao projeto; não precisa continuar no caminho original.
Não é possível reconstruir automaticamente os sketches e o histórico do CAD que
produziu o STEP. Salve também `.mcad` para continuar a edição paramétrica.
