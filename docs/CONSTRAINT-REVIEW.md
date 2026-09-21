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
não representa um editor de conjuntos conflitantes rejeitados pelo solver nem
adiciona suporte a restrições de curvas. O realce é temporário, não é uma nova
seleção editável no documento.
