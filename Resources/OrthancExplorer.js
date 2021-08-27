$('#lookup').live('pagebeforeshow', function() {
  $('#open-tcia-client').remove();
  
  var b = $('<fieldset>')
      .attr('id', 'open-tcia-client')
      .addClass('ui-grid-b')
      .append($('<div>')
              .addClass('ui-block-a'))
      .append($('<div>')
              .addClass('ui-block-b')
              .append($('<a>')
                      .attr('data-role', 'button')
                      .attr('href', '#')
                      .attr('data-icon', 'forward')
                      .attr('data-theme', 'a')
                      .text('The Cancer Imaging Archive')
                      .button()
                      .click(function(e) {
                        window.open('../tcia/app/index.html');
                      })));
  
  b.insertAfter($('#lookup-result'));
});
