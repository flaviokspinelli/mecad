# Revisão de restrições

Selecione um sketch e use **Constraints → Revisar / remover restrições…**
(também disponível nas propriedades de sketches com restrições persistidas).

O painel mostra os graus de liberdade, as relações existentes e quais o solver
classifica como redundantes. Selecionar uma linha da lista realça a geometria:
horizontal/vertical realçam o segmento; fixação realça o ponto; distâncias e
coincidências realçam os dois pontos. Cotas X/Y mostram o valor resolvido em mm.

Consultar ou cancelar não modifica o documento. **Remover selecionada** confirma
uma única remoção, que pode ser desfeita; fórmulas vinculadas à relação são
tratadas pela mesma transação existente no modelo.

Redundância não é necessariamente erro: relações compatíveis podem repetir uma
condição. Não há remoção automática. Este painel inspeciona relações já aceitas;
não adiciona suporte a restrições de curvas. O realce é temporário, não é uma nova
seleção editável no documento.

## Pontos presos e móveis

Ao editar um sketch com sistema de restrições persistido, pontos dourados estão
totalmente presos; pontos azuis ainda têm uma ou duas direções independentes de
movimento. A legenda mostra se o conjunto está sub-restrito ou totalmente restrito
e informa os graus de liberdade globais. Isso é calculado pela solução combinada:
um ponto pode ficar preso por cotas e outras relações, sem uma relação Fixar.

Os graus de liberdade locais não devem ser somados: dois pontos coincidentes
podem mover-se juntos e compartilhar os mesmos graus de liberdade. O diagnóstico
é recalculado quando a geometria é atualizada, não em cada quadro da pintura.
O indicador cobre o solver afim de linhas atual; círculos/arcos e restrições
angulares não recebem uma classificação fictícia. As cores não alteram seleção,
geometria, restrições ou comportamento do arraste.

## Ao tentar adicionar uma restrição incompatível

Nos comandos horizontal, vertical, fixação e cotas X/Y, o solver verifica a
tentativa antes de modificar o documento. Um painel identifica a relação nova e
as relações que participaram do conflito. Selecionar uma delas realça a geometria
original; nenhum resultado geométrico inconsistente é exibido como prévia válida.

**Revisar restrições existentes…** abre o painel de revisão. Remova somente uma
relação que faça sentido para o projeto e depois repita o comando desejado. A
tentativa rejeitada não fica pendente e não é reaplicada automaticamente. Cancelar
qualquer painel preserva o documento; a remoção confirmada continua reversível.

A lista de conflitos é um conjunto de candidatas, não necessariamente o conjunto
mínimo. Erros de contorno (colapso/auto-interseção), edição direta de fórmulas e
outros caminhos de reconstrução continuam com o diagnóstico existente; ainda não
usam este fluxo visual.
