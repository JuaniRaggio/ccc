module procesar_tb;
  logic signed [31:0] x, cuadrado, absoluto;

  procesar dut (.x(x), .cuadrado(cuadrado), .absoluto(absoluto));

  initial begin
    x = -7;
    #1;
    if (cuadrado !== 49 || absoluto !== 7) $fatal(1, "procesar(-7) = (%0d, %0d)", cuadrado, absoluto);
    $display("procesar(%0d) = (cuadrado=%0d, absoluto=%0d)", x, cuadrado, absoluto);
    x = 4;
    #1;
    if (cuadrado !== 16 || absoluto !== 4) $fatal(1, "procesar(4) = (%0d, %0d)", cuadrado, absoluto);
    $display("procesar(%0d) = (cuadrado=%0d, absoluto=%0d)", x, cuadrado, absoluto);
    $display("PASS");
    $finish;
  end
endmodule
