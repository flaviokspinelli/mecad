# Guia rápido — MecaCAD 0.2

## Primeira peça

1. Clique em **Create Sketch**, no canto esquerdo da barra de criação.
2. Clique no plano **XY**, **XZ** ou **YZ** exibido na área 3D. Também é possível
   clicar em uma face plana alinhada a esses planos; seu offset será usado.
3. A ferramenta de retângulo começa ativa. Pressione no primeiro canto, arraste e
   solte no segundo. Também é possível clicar nos dois cantos separadamente.
4. Se precisar de medidas exatas, selecione o perfil e pressione **D**. Ajuste as
   dimensões e clique em **Apply**. Isso é opcional; selecionar não abre o painel.
5. Clique em **Finish Sketch** no lado direito da barra.
6. Selecione o interior do perfil, pressione **E** e arraste a seta azul para dar
   altura. A peça aparece em prévia. **Enter** confirma; **Esc** cancela. A distância
   exata também pode ser digitada no pequeno painel da operação.
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
| Selecionar corpo, interior ou contorno de sketch | Clique esquerdo |
| Extrudar com prévia | E, arrastar a seta azul, Enter |
| Mover com prévia | M, arrastar uma seta X/Y/Z, Enter |
| Cancelar prévia sem gravar uma operação | Esc |
| Trocar vista com animação | Clique em uma face, aresta ou canto do cubo |
| Orbitar pelo cubo, inclusive durante sketch | Segurar o cubo com botão esquerdo e arrastar |
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

A câmera começa perpendicular ao plano no sketch; arrastar o cubo permite
inspecionar em 3D sem mudar o plano do desenho. Clicar numa face do cubo durante
o sketch retorna à orientação do plano de trabalho. O atalho de dimensão
leva ao painel de parâmetros do objeto selecionado. As combinações de pan e
órbita seguem a [referência de navegação da Autodesk](https://www.autodesk.com/products/fusion-360/blog/quick-tip-pan-zoom-orbit-preferences/).

## Sketches e planos

### Encaixe inteligente (ímã)

Ativo por padrão. Aproxime o cursor de extremidades, pontos médios, centros,
interseções de segmentos ou origem: um marcador verde identifica o encaixe.
Guias tracejadas indicam alinhamento horizontal/vertical com pontos existentes
ou com os pontos do perfil em andamento. Funciona tanto por clique quanto por arraste.

A tolerância é visual (10 pixels para adquirir um ponto, até 15 para soltá-lo),
independente do zoom. Pontos geométricos têm prioridade sobre Snap to 1 mm.
Desative **Encaixe inteligente** no menu da grade para desenhar livremente;
o encaixe na grade é uma opção independente.

São usadas referências de sketches exibidos no mesmo plano e offset. Não inclui
projeção de arestas de sólidos, tangência, interseções com curvas ou um solucionador
de restrições: o alinhamento posiciona o novo ponto, mas não o vincula a futuras
alterações da geometria usada como referência.

### Perfis

Cada retângulo, círculo, arco ou cadeia de linhas cria um sketch independente.
Uma polilinha fecha ao clicar novamente no primeiro ponto ou usar Shift+Enter.
Arcos usam três cliques: início, ponto intermediário e fim. Arcos e linhas abertas
podem ser exportados em DXF, mas não extrudados como sólidos.

Retângulos e círculos mostram prévia ao pressionar e arrastar; soltar confirma.
No círculo, pressione no centro e arraste até a borda. A ferramenta Line aceita
o primeiro segmento por arraste, seguido dos demais vértices por clique. Enter
conclui a cadeia; Esc descarta o perfil em andamento. Arrastar para navegar com
Alt ou botão central não cria entidades de sketch.

O snap arredonda as coordenadas ao milímetro; desative **Snap 1 mm** para desenho
livre. Para precisão final, digite os valores no painel. A grade visual adapta o
espaçamento ao zoom e não altera a unidade dos parâmetros.
O controle de snap está no menu de grade da barra de navegação inferior.
Os manipuladores de extrusão e movimento arredondam a 0,1 mm quando o snap está ativo.

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

Novas operações Mover/copy giram em torno do centro da caixa envolvente da peça,
seguido da translação. Operações antigas preservam seu pivô. A opção
**Create Copy** preserva o corpo original. A visibilidade é controlada no Browser.
Pressione **M** e arraste as setas coloridas para transladar. O botão **Precise
values / rotation** expande as medidas e os controles numéricos de rotação.
Para girar pelo mouse: **M → Girar pelo mouse**, escolha X, Y ou Z e arraste
o anel azul. Também é possível digitar o ângulo. Funciona em sólidos e STL.
Durante a prévia você pode continuar orbitando, aproximando e deslocando a vista.
A operação só entra no histórico ao confirmar; Escape descarta a prévia.

## Editar, salvar e recuperar

Dê duplo clique na operação no histórico, ou use **Edit Feature** no menu do
Browser, mude seus parâmetros e clique em Apply. As operações seguintes são recalculadas. Uma edição inválida preserva
o documento anterior. Não é possível excluir uma operação ainda referenciada:
remova primeiro suas dependentes.

**Apagar peça** (Delete/Backspace) remove o corpo final da cena, sem revelar as
versões anteriores. A remoção fica registrada no histórico e pode ser desfeita.
Para desfazer somente a operação selecionada, use **Edit → Voltar uma etapa da
peça**, também disponível no menu de contexto do Browser. Essa ação continua
protegida quando existem operações dependentes.

O projeto `.mcad` contém parâmetros e histórico. O aplicativo oferece salvar ao
fechar um documento alterado. A recuperação automática é gravada a cada 30 segundos
na pasta de dados do MecaCAD e oferecida na próxima abertura após interrupção.
Ela não substitui salvar regularmente; o intervalo mais recente pode não ter sido gravado.

## Editar medidas no desenho

### Selecionar linhas e vértices

No modo automático, clique próximo de um canto para selecionar o vértice, no meio
de uma linha para selecionar a aresta, ou no interior para selecionar o perfil.
O alvo sob o mouse fica amarelo; a seleção confirmada fica azul. Use **View → Seleção**
ou **SELECT** para filtrar por tipo. O Browser seleciona o objeto inteiro. Esc ou
um clique vazio limpa a seleção. Funciona em sketches e sólidos CAD; malhas STL
ainda são selecionadas por inteiro. **Measure** mostra coordenadas de vértices ou
comprimento aproximado de arestas. Selecionar não permite ainda arrastar/excluir
vértices individualmente; comandos de corpo pedem seleção do objeto completo.

### Alterar cotas

No sketch, selecione o perfil e clique no número dourado da cota. Digite a
medida em milímetros (vírgula ou ponto decimal) e pressione Enter. Esc cancela;
clicar fora também cancela. Retângulos permitem largura e altura; círculos,
diâmetro. As operações dependentes são reconstruídas e a edição pode ser desfeita.
Valores inválidos mantêm o campo aberto, sem alterar a peça. Duplo clique no
perfil reabre seu sketch. Outros tipos ainda usam o painel de propriedades.

## Exportação de arquivos

Para importar STL, use **File → Import STEP / STL** ou **Insert → Import STEP / STL**.
STL binário e ASCII são aceitos. **File → Open** abre STL como um novo documento.
A malha é interpretada em milímetros e incorporada ao salvar `.mcad`, sem depender
do STL original. Limites atuais: 50 MB e 1 milhão de triângulos. Importar não
converte malha em sólido CAD: operações de sólidos e exportação STEP de malhas
ainda não estão disponíveis. É possível visualizar e reexportar em STL.

- **STEP**: sólido selecionado; sem seleção de sólido, exporta os corpos finais visíveis.
- **STL**: mesma regra de seleção. É uma malha de triângulos, sem histórico. As
  coordenadas são em mm; confirme mm ao abrir no fatiador, pois STL não registra unidade.
- **DXF**: selecione um sketch no Browser. O arquivo contém seu desenho 2D nas
  coordenadas locais do plano, com indicação de milímetros.

O STEP importado é incorporado ao projeto; não precisa continuar no caminho original.
Não é possível reconstruir automaticamente os sketches e o histórico do CAD que
produziu o STEP. Salve também `.mcad` para continuar a edição paramétrica.
