$( '#buttons' ).live( 'pageinit',function(event){
  $( "button#refresh" ).click( function(e,ui) {
    $.get("/buttons_list.shtml",function(data) {
      $("#buttons_list").html(data).listview('refresh');
      });
  });
});
$( '#lights' ).live( 'pageinit',function(event){
  $( "select.light" ).change( function(e, ui) {
    $.get("/set_pin.shtml?pin="+e.target.name+"&val="+$(e.target).val()); 
    });
});
$( '#sensors' ).live( 'pageinit',function(event){
  $( "button#refresh" ).click( function(e,ui) {
    $.get("/sensors_list.shtml",function(data) {
      $("#sensors_list").html(data).listview('refresh');
      });
  });
});
