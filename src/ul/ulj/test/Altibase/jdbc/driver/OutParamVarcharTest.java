package Altibase.jdbc.driver;

import java.sql.CallableStatement;
import java.sql.SQLException;
import java.sql.Types;

public class OutParamVarcharTest extends AltibaseTestCase
{
    protected String[] getCleanQueries()
    {
        return new String[] {
                "DROP TABLE t1",
                "DROP PROCEDURE PSM1",
        };
    }

    protected String[] getInitQueries()
    {
        return new String[] {
                "CREATE TABLE t1 (c1 VARCHAR)",
                "CREATE OR REPLACE PROCEDURE psm1"
                +"("
                    +"v1 IN VARCHAR(32000),"
                    +"v2 OUT VARCHAR(32000)"
                +")"
                +"AS BEGIN"
                    +" UPDATE t1 SET c1 = 7;"
                    +" v2 := v1;"
                    +" PRINTLN(v1);"
                +"END",
        };
    }

    // from BUG-32521
    public void testOutVarchar() throws SQLException
    {
        String sStrVal = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" +
                         "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890ABCDEFGHIJ";

        CallableStatement sCallStmt = connection().prepareCall("{call PSM1(?,?)}");
        sCallStmt.setString(1,sStrVal);
        sCallStmt.registerOutParameter(2, Types.VARCHAR);
        sCallStmt.execute();
        assertEquals(sStrVal, sCallStmt.getString(2));
        sCallStmt.close();
    }
}