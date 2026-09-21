# Preferências persistentes

O aplicativo guarda quatro escolhas locais entre sessões:

- fundo claro/escuro, em View → Light canvas ou no menu de visualização;
- arestas visíveis, no menu de visualização;
- snap da grade, no menu de grade;
- encaixe inteligente, no menu de grade.

As escolhas são gravadas em `preferences.ini`, na pasta de configuração do
aplicativo fornecida pelo macOS/Qt. Não ficam no `.mcad`, não marcam o desenho
como alterado e não entram no histórico de desfazer. Testes usam arquivos isolados
em diretórios temporários, sem gravar nas preferências normais do usuário.

Ausência de uma chave ou valor booleano inválido usa o padrão: fundo escuro,
arestas, snap e encaixe inteligente ligados. Se não for possível gravar, a escolha
continua valendo na sessão e a barra de status informa a falha. Não há promessa de
sincronização ao vivo entre várias instâncias abertas.

## Atalhos de modelagem

Help → Configurar atalhos permite alterar retângulo, círculo, linha, cota,
extrusão, furo, movimento, medição, busca e enquadramento. Use uma letra A–Z,
opcionalmente com modificadores, ou apague a combinação para desativá-la.
Conflitos com outros comandos (inclusive salvar/abrir) bloqueiam OK.
Enter/Esc, Delete e navegação não são remapeados por esse painel.

Cancelar não aplica nada. Restaurar padrões preenche o painel; confirme com OK
para aplicar. Atalhos são guardados no mesmo arquivo de preferências, fora do
documento. Configuração inválida na abertura usa os padrões e informa o problema.

Help → Guia de uso e limites reúne fluxos de sketch/modelagem, cotas/restrições,
navegação e limitações atuais. A aba Teclado e mouse consulta os atalhos em uso
na abertura do guia, inclusive comandos que ficaram sem tecla atribuída.

Unidades, idioma e perfis de navegação ainda não fazem parte dessas preferências
persistentes. Portanto PROD-05 permanece parcial.
