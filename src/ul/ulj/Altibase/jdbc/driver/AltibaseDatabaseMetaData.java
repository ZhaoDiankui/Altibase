/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

package Altibase.jdbc.driver;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.StringUtils;

import java.sql.*;

public class AltibaseDatabaseMetaData implements DatabaseMetaData
{
    private static final int         QCM_NOTNULL             = 0;
    private static final int         QCM_UNIQUE              = 1;
    private static final int         QCM_PRIMARY_KEY         = 2;
    private static final int         QCM_FOREIGN_KEY         = 3;
    private static final int         QCM_CHECK               = 4;

    private static final int         QC_MAX_OBJECT_NAME_LEN  = 128;
    private static final int         QC_MAX_COLUMN_COUNT     = 1024;
    private static final int         QC_MAX_KEY_COLUMN_COUNT = 32;
    private static final int         QC_MAX_REF_TABLE_CNT    = 32;

    private static final int         MAX_NAME_LEN            = 45;
    private static final int         MAX_FILE_NAME_LEN       = 256;
    private static final int         MAX_ERROR_MSG_LEN       = 4 * 1024;

    private static final int         VERSION_MAJOR_IDX       = 0;
    private static final int         VERSION_MINOR_IDX       = 1;

    private final AltibaseConnection mConn;
    private StringBuffer             mSql;
    private int[]                    mDBVersion;

    AltibaseDatabaseMetaData(AltibaseConnection aConnection) throws SQLException
    {
        mConn = aConnection;
        mSql = new StringBuffer(1024);
    }
	
    public boolean allProceduresAreCallable() throws SQLException
    {
        return true;
    }

    public boolean allTablesAreSelectable() throws SQLException
    {
        return true;
    }

    public boolean dataDefinitionCausesTransactionCommit() throws SQLException
    {
        return true;
    }

    public boolean dataDefinitionIgnoredInTransactions() throws SQLException
    {
        return false;
    }

    public boolean deletesAreDetected(int aType) throws SQLException
    {
        return (aType == ResultSet.TYPE_SCROLL_SENSITIVE);
    }

    public boolean doesMaxRowSizeIncludeBlobs() throws SQLException
    {   
        return true;
    }

    public synchronized ResultSet getAttributes(String aCatalog, String aSchemaPattern, String aTypeNamePattern, String aAttributeNamePattern) throws SQLException
    {
    	mSql.setLength(0);
        mSql.append("SELECT '' TYPE_CAT,'' TYPE_SCHEM, '' TYPE_NAME,'' ATTR_NAME,0 DATA_TYPE, ''  ATTR_TYPE_NAME,0 ATTR_SIZE,0 DECIMAL_DIGITS,0 NUM_PREC_RADIX,");
        mSql.append("NULL NULLABLE,'' REMARKS,'' ATTR_DEF,NULL SQL_DATA_TYPE, NULL SQL_DATETIME_SUB,NULL CHAR_OCTET_LENGTH,NULL ORDINAL_POSITION,'YES' IS_NULLABLE,");
        mSql.append("'' SCOPE_CATALOG,'' SCOPE_SCHEMA,'' SCOPE_TABLE,'' SOURCE_DATA_TYPE from DUAL WHERE 1=0");
        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public synchronized ResultSet getBestRowIdentifier(String aCatalog, String aSchema, String aTable, int aScope, boolean aNullable) throws SQLException
    {
        mSql.setLength(0);
        mSql.append("SELECT 2 as SCOPE,c.column_name as COLUMN_NAME,t.sql_data_type as DATA_TYPE,t.type_name as TYPE_NAME,decode(c.precision,0,decode(c.data_type, 1, c.precision, 12, c.precision, -8, c.precision, -9, c.precision, 60, c.precision, 61, c.precision, t.COLUMN_SIZE),c.precision) as COLUMN_SIZE,c.precision as BUFFER_LENGTH,c.scale as DECIMAL_DIGITS,1 as PSEUDO_COLUMN");
        mSql.append(" FROM (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,system_.SYS_CONSTRAINT_COLUMNS_ a,system_.SYS_CONSTRAINTS_ b,system_.SYS_COLUMNS_ c,system_.SYS_TABLES_ d,system_.SYS_USERS_ e,X$DATATYPE t");
        mSql.append(" WHERE a.CONSTRAINT_ID=b.CONSTRAINT_ID and b.constraint_type=3 and a.COLUMN_ID=c.COLUMN_ID and b.table_id=d.table_id and d.user_id=e.user_id");
        if (!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" and db.TABLE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aSchema))
        {
            mSql.append(" and e.user_name='");
            mSql.append(aSchema);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aTable))
        {
            mSql.append(" and d.table_name='");
            mSql.append(aTable);
            mSql.append('\'');
        }
        if (aNullable)
        {
            mSql.append(" and C.IS_NULLABLE='T'");
        }
        else
        {
            mSql.append(" and C.IS_NULLABLE='F'");
        }
        mSql.append(" and 0 <= ");
        mSql.append(aScope);
        mSql.append(" and c.data_type=t.data_type");
        mSql.append(" ORDER BY SCOPE");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public String getCatalogSeparator() throws SQLException
    {
    	return ".";
    }

    public String getCatalogTerm() throws SQLException
    {
    	return "database";
    }

    public synchronized ResultSet getCatalogs() throws SQLException
    {
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery("SELECT DB_NAME as TABLE_CAT FROM V$DATABASE");
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public ResultSet getColumnPrivileges(String aCatalog, String aSchema, String aTable, String aColumnNamePattern) throws SQLException    
    {	 
    	Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "To get a description of the access rights for a table's columns.");
        return null;
    }

    // BUGBUG (2013-01-15) aTableNamePattern�� SYNONYM�� ���� �ȵȴ�.
    public synchronized ResultSet getColumns(String aCatalog, String aSchemaPattern, String aTableNamePattern, String aColumnNamePattern) throws SQLException
    {
        mSql.setLength(0);
        // BUG-44466 ORDINAL_POSITION �� 1���� �����ϱ� ������ ������ �����Ѵ�.
        mSql.append("SELECT '' as TABLE_CAT, c.user_name as TABLE_SCHEM, b.table_name as TABLE_NAME, a.column_name as COLUMN_NAME, nvl2(d.column_id,3010,decode(a.data_type, 9, 93, a.data_type)) as DATA_TYPE, ");
        mSql.append("nvl2(d.column_id,'TIMESTAMP', decode(a.data_type, 60, CHAR'CHAR', 61, VARCHAR'VARCHAR', t.type_name)) as TYPE_NAME, ");
        mSql.append("decode(a.precision,0,decode(a.data_type, 1, a.precision, 12, a.precision, -8, a.precision, -9, a.precision, 60, a.precision, 61, a.precision, t.column_size),a.precision) as COLUMN_SIZE, decode(a.size, 4294967295, 2147483647, a.size ) as BUFFER_LENGTH,");
        mSql.append("a.scale as  DECIMAL_DIGITS, t.NUM_PREC_RADIX as NUM_PREC_RADIX, decode(a.IS_NULLABLE,'F',0,1) as NULLABLE, a.STORE_TYPE as REMARKS, a.DEFAULT_VAL as COLUMN_DEF, ");
        mSql.append("t.SQL_DATA_TYPE as SQL_DATA_TYPE, t.SQL_DATETIME_SUB as SQL_DATETIME_SUB, decode(a.size, 4294967295, 2147483647, a.size ) as CHAR_OCTET_LENGTH, a.column_order + 1 as ORDINAL_POSITION, ");
        mSql.append("decode(a.IS_NULLABLE,'F','NO','YES') as IS_NULLABLE, '' as SCOPE_CATALOG, '' as SCOPE_SCHEMA, '' as SCOPE_TABLE, null as  SOURCE_DATA_TYPE ");
        mSql.append("from ( select /*+index(system_.sys_columns_ SYS_COLUMNS_INDEX3)*/ * from system_.sys_columns_ where (table_id, column_order) in (select a.table_id, a.column_order from system_.sys_columns_ a, system_.sys_tables_ b, system_.sys_users_ c where a.table_id=b.table_id and b.user_id=c.user_id and a.is_hidden = 'F' and b.table_type in ('T','V','Q','M') ");
        if (!StringUtils.isEmpty(aSchemaPattern))
        {
            mSql.append("and c.user_name LIKE '");
            mSql.append(aSchemaPattern);
            mSql.append("' escape '\\' ");
        }
        if (!StringUtils.isEmpty(aTableNamePattern))
        {
            mSql.append(" and b.table_name LIKE '");
            mSql.append(aTableNamePattern);
            mSql.append("' escape '\\'");
        }
        if (!StringUtils.isEmpty(aColumnNamePattern))
        {
            mSql.append(" and a.column_name LIKE '");
            mSql.append(aColumnNamePattern);
            mSql.append("' escape '\\'");
        }
        
        mSql.append(")) a left outer join system_.sys_constraints_ e on a.table_id = e.table_id and e.constraint_type=5 left outer join system_.sys_constraint_columns_ d on e.constraint_id = d.constraint_id and a.column_id = d.column_id left outer join system_.sys_tables_ b on a.table_id = b.table_id left outer join system_.sys_users_ c on a.user_id = c.user_id left outer join X$DATATYPE t on a.data_type = t.data_type order by a.column_id");
        
        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public Connection getConnection() throws SQLException
    {
        return mConn;
    }

    public synchronized ResultSet getCrossReference(String aPrimaryCatalog, String aPrimarySchema, String aPrimaryTable, String aForeignCatalog, String aForeignSchema, String aForeignTable) throws SQLException
    {    	
        return keys_query(aPrimaryCatalog, aPrimarySchema, aPrimaryTable, aForeignCatalog, aForeignSchema, aForeignTable, "FKTABLE_CAT, FKTABLE_SCHEM, FKTABLE_NAME, KEY_SEQ");
    }

    public int getDatabaseMajorVersion() throws SQLException
    {
        ensureDatabaseVersion();
    	return mDBVersion[VERSION_MAJOR_IDX];
    }

    public int getDatabaseMinorVersion() throws SQLException
    {
        ensureDatabaseVersion();
        return mDBVersion[VERSION_MINOR_IDX];
    }

    private void ensureDatabaseVersion() throws SQLException
    {
        if (mDBVersion != null)
        {
            return;
        }

        String[] sDBVers = mConn.getDatabaseVersion().split("\\.");
        mDBVersion = new int[3];
        mDBVersion[VERSION_MAJOR_IDX] = Integer.parseInt(sDBVers[VERSION_MAJOR_IDX]);
        mDBVersion[VERSION_MINOR_IDX] = Integer.parseInt(sDBVers[VERSION_MINOR_IDX]);
    }

    public String getDatabaseProductName() throws SQLException
    {
        return "Altibase";
    }

    public String getDatabaseProductVersion() throws SQLException
    {
        return mConn.getDatabaseVersion();
    }

    public int getDefaultTransactionIsolation() throws SQLException
    {
    	return AltibaseConnection.TRANSACTION_READ_COMMITTED;
    }

    public int getDriverMajorVersion()
    {
        return AltibaseVersion.ALTIBASE_MAJOR_VERSION;
    }

    public int getDriverMinorVersion()
    {
        return AltibaseVersion.ALTIBASE_MINOR_VERSION;
    }

    public String getDriverName() throws SQLException
    {
    	return "Altibase JDBC driver";
    }

    public String getDriverVersion() throws SQLException
    {
    	return AltibaseVersion.ALTIBASE_VERSION_STRING;
    }

    public synchronized ResultSet getExportedKeys(String aCatalog, String aSchema, String aTable) throws SQLException
    {
        return keys_query(aCatalog, aSchema, aTable, null, null, null, "FKTABLE_CAT, FKTABLE_SCHEM, FKTABLE_NAME, KEY_SEQ");
    }

    public String getExtraNameCharacters() throws SQLException
    {
        return "$";
    }

    public String getIdentifierQuoteString() throws SQLException
    {
    	return "\"";
    }

    public synchronized ResultSet getImportedKeys(String aCatalog, String aSchema, String aTable) throws SQLException
    {
        return keys_query(null, null, null, aCatalog, aSchema, aTable, "PKTABLE_CAT, PKTABLE_SCHEM, PKTABLE_NAME, KEY_SEQ");
    }

    public synchronized ResultSet getIndexInfo(String aCatalog, String aSchema, String aTable, boolean aUnique, boolean aApproximate) throws SQLException
    {
        if (StringUtils.isEmpty(aTable))
        {
          Error.throwSQLException(ErrorDef.NO_TABLE_NAME_SPECIFIED);
        }

        mSql.setLength(0);
        mSql.append("select db.TABLE_CAT,E.user_name as TABLE_SCHEM,D.table_name as TABLE_NAME,decode(a.is_unique,'T',0,'F',1,1) as NON_UNIQUE,'' as INDEX_QUALIFIER,a.index_name as INDEX_NAME,a.index_type as TYPE,b.index_col_order+1 as ORDINAL_POSITION,C.column_name as COLUMN_NAME, B.SORT_ORDER as ASC_OR_DESC,0 as CARDINALITY,0 as PAGES,'' as FILTER_CONDITION");
        // mSql.append(" a.index_id");
        mSql.append(" FROM (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,");
        mSql.append("system_.sys_indices_ A,");
        mSql.append("system_.sys_index_columns_ B,");
        mSql.append("system_.sys_columns_ C,");
        mSql.append("system_.sys_tables_ D,");
        mSql.append("system_.sys_users_ E");
        mSql.append(" WHERE a.table_id=b.table_id and c.column_id = b.column_id and b.index_id  = a.index_id and a.table_id = d.table_id and d.user_id = e.user_id");
        if (!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" and db.TABLE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aSchema))
        {
            mSql.append(" and e.user_name='");
            mSql.append(aSchema);
            mSql.append('\'');
        }
        // table
        {
            mSql.append(" and D.table_name='");
            mSql.append(aTable);
            mSql.append('\'');
        }
        if (aUnique == true)
        {
            mSql.append(" and a.is_unique = 'T'");
        }
        mSql.append(" order by NON_UNIQUE,TYPE,INDEX_NAME,ORDINAL_POSITION");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public int getJDBCMajorVersion() throws SQLException
    {
        return AltibaseVersion.ALTIBASE_MAJOR_VERSION;
    }

    public int getJDBCMinorVersion() throws SQLException
    {
    	return AltibaseVersion.ALTIBASE_MINOR_VERSION;
    }

    public int getMaxBinaryLiteralLength() throws SQLException
    {
        return 32000;
    }

    public int getMaxCatalogNameLength() throws SQLException
    {
        return MAX_NAME_LEN;
    }

    public int getMaxCharLiteralLength() throws SQLException
    {
        return 32000;
    }

    public int getMaxColumnNameLength() throws SQLException
    {
        return QC_MAX_OBJECT_NAME_LEN;
    }

    public int getMaxColumnsInGroupBy() throws SQLException
    {
        return QC_MAX_COLUMN_COUNT;
    }

    public int getMaxColumnsInIndex() throws SQLException
    {
        return QC_MAX_KEY_COLUMN_COUNT;
    }

    public int getMaxColumnsInOrderBy() throws SQLException
    {
        return QC_MAX_COLUMN_COUNT;
    }

    public int getMaxColumnsInSelect() throws SQLException
    {
    	return QC_MAX_COLUMN_COUNT;
    }

    public int getMaxColumnsInTable() throws SQLException
    {
        return QC_MAX_COLUMN_COUNT;
    }

    public synchronized int getMaxConnections() throws SQLException
    {
    	int max = 0;    	
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery("select VALUE1 from V$PROPERTY where name = 'MAX_CLIENT'");
        if(rs.next())
        {
            max = rs.getInt(1);
        }
        rs.close();
        stmt.close();
        return max;
    }

    public int getMaxCursorNameLength() throws SQLException
    {
        return MAX_NAME_LEN;
    }

    public int getMaxIndexLength() throws SQLException
    {
        return QC_MAX_KEY_COLUMN_COUNT;
    }

    public int getMaxProcedureNameLength() throws SQLException
    {
    	return QC_MAX_OBJECT_NAME_LEN;
    }

    public int getMaxRowSize() throws SQLException
    {
    	return 0;
    }

    public int getMaxSchemaNameLength() throws SQLException
    {
        return QC_MAX_OBJECT_NAME_LEN;
    }

    public int getMaxStatementLength() throws SQLException
    {
        return 0;
    }

    public int getMaxStatements() throws SQLException
    {
    	return 0;
    }

    public int getMaxTableNameLength() throws SQLException
    {
        return QC_MAX_OBJECT_NAME_LEN;
    }

    public int getMaxTablesInSelect() throws SQLException
    {
    	return QC_MAX_REF_TABLE_CNT;
    }

    public int getMaxUserNameLength() throws SQLException
    {
        return QC_MAX_OBJECT_NAME_LEN;
    }

    public String getNumericFunctions() throws SQLException
    {
    	return "ABS," + "ACOS," + "ADD2," + "ASIN," + "ATAN," + "ATAN2," + "AVG," + "COS," + "COSH," + "DIV2," + "EXP," + "FLOOR," + "GREATEST," + "LN," + "LOG," + "LOWER," + "MAX," + "MIN," + "MINUS," + "MOD," + "MUL2," + "NVL," + "NVL2," + "POWER," + "RANDOM," + "ROUND," + "SIGN," + "SIN," + "SINH," + "SQRT," + "SUM," + "TAN," + "TANH," + "VARIANCE";
    }

    public synchronized ResultSet getPrimaryKeys(String aCatalog, String aSchema, String aTable) throws SQLException
    {
        mSql.setLength(0);
        mSql.append("select db.TABLE_CAT, e.user_name as TABLE_SCHEM,d.table_name as TABLE_NAME,c.column_name as COLUMN_NAME,a.CONSTRAINT_COL_ORDER+1 as KEY_SEQ,b.CONSTRAINT_NAME as PK_NAME,b.index_id,b.column_cnt");
        mSql.append(" from (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,");
        mSql.append("system_.SYS_CONSTRAINT_COLUMNS_ a,");
        mSql.append("system_.SYS_CONSTRAINTS_ b,");
        mSql.append("system_.SYS_COLUMNS_ c,");
        mSql.append("system_.SYS_TABLES_ d,");
        mSql.append("system_.SYS_USERS_ e");
        mSql.append(" where a.CONSTRAINT_ID = b.CONSTRAINT_ID and b.constraint_type = 3 and a.COLUMN_ID = c.COLUMN_ID and b.table_id  = d.table_id and d.user_id = e.user_id");
        if (!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" and db.TABLE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aSchema))
        {
            mSql.append(" and e.user_name='");
            mSql.append(aSchema);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aTable))
        {
            mSql.append(" and d.table_name='");
            mSql.append(aTable);
            mSql.append('\'');
        }
        mSql.append(" order by COLUMN_NAME");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public synchronized ResultSet getProcedureColumns(String aCatalog, String aSchemaPattern, String aProcedureNamePattern, String aColumnNamePattern) throws SQLException
    {
        mSql.setLength(0);

        if (!StringUtils.isEmpty(aCatalog))
        {
            aCatalog = " AND db.PROCEDURE_CAT='" + aCatalog + "'";
        }
        else
        {
            aCatalog = "";
        }
        if (!StringUtils.isEmpty(aSchemaPattern) && aSchemaPattern.compareTo("%") != 0)
        {
            aSchemaPattern = " AND d.user_name  LIKE '" + aSchemaPattern + "'  escape '\\'";
        }
        else
        {
            aSchemaPattern = "";
        }
        if (!StringUtils.isEmpty(aProcedureNamePattern) && aProcedureNamePattern.compareTo("%") != 0)
        {
            aProcedureNamePattern = " AND  b.proc_name LIKE '" + aProcedureNamePattern + "' escape '\\'";
        }
        else
        {
            aProcedureNamePattern = "";
        }
        if (!StringUtils.isEmpty(aColumnNamePattern))
        {
            aColumnNamePattern = " AND a.para_name LIKE '" + aColumnNamePattern + "' escape '\\'";
        }
        else
        {
            aColumnNamePattern = "";
        }
        
        mSql.append("SELECT db.PROCEDURE_CAT,");
        mSql.append("d.user_name as PROCEDURE_SCHEM,");
        mSql.append("b.PROC_NAME as PROCEDURE_NAME,");
        mSql.append("'RETURN_VALUE' as COLUMN_NAME,");
        mSql.append("5 as COLUMN_TYPE,");
        mSql.append("b.RETURN_DATA_TYPE as DATA_TYPE,");
        mSql.append("t.type_name as TYPE_NAME,");
        mSql.append("decode(b.RETURN_PRECISION,0,decode(b.RETURN_DATA_TYPE, 1, b.RETURN_PRECISION, 12, b.RETURN_PRECISION, -8, b.RETURN_PRECISION, -9, b.RETURN_PRECISION, 60, b.RETURN_PRECISION, 61, b.RETURN_PRECISION, t.column_size),b.RETURN_PRECISION) as PRECISION,");
        mSql.append("decode(b.RETURN_SIZE,0,b.RETURN_PRECISION,b.RETURN_SIZE) as LENGTH,");
        mSql.append("b.RETURN_SCALE as SCALE,");
        mSql.append("t.NUM_PREC_RADIX as RADIX,");
        mSql.append("2 as NULLABLE,");
        mSql.append("'RETURN VALUE' as REMARKS,");
        mSql.append("'' as COLUMN_DEF,");
        mSql.append("t.SQL_DATA_TYPE as SQL_DATA_TYPE,");
        mSql.append("t.SQL_DATETIME_SUB as SQL_DATETIME_SUB,");
        mSql.append("decode(b.RETURN_SIZE,0,b.RETURN_PRECISION, b.RETURN_SIZE) as CHAR_OCTET_LENGTH,");
        mSql.append("0 as ORDINAL_POSITION,");
        mSql.append("1 as IS_NULLABLE");
        mSql.append(" FROM (SELECT DB_NAME as PROCEDURE_CAT FROM V$DATABASE) db,");
        mSql.append("X$DATATYPE t,");
        mSql.append("system_.sys_procedures_ b,");
        mSql.append("system_.sys_users_ d");
        mSql.append(" WHERE b.OBJECT_TYPE = 1 AND b.user_id=d.user_id AND t.data_type=b.RETURN_DATA_TYPE");
        mSql.append(aCatalog);
        mSql.append(aSchemaPattern);
        mSql.append(aProcedureNamePattern);
        mSql.append(" UNION ");
        mSql.append("SELECT db.PROCEDURE_CAT,");
        mSql.append("d.user_name as PROCEDURE_SCHEM,");
        mSql.append("b.proc_name as PROCEDURE_NAME,");
        mSql.append("a.para_name as COLUMN_NAME,");
        mSql.append("decode(a.data_type,1000004,3,decode(a.inout_type,0,1,2,2,1,4,0)) as COLUMN_TYPE,");
        mSql.append("NVL2(t.TYPE_NAME,a.data_type,1111) as DATA_TYPE,");
        mSql.append("NVL(t.TYPE_NAME,decode(a.data_type,1000004,'REF CURSOR','OTHER')) as TYPE_NAME,");
        mSql.append("decode(a.precision,0,decode(a.data_type, 1, a.precision, 12, a.precision, -8, a.precision, -9, a.precision, 60, a.precision, 61, a.precision, t.COLUMN_SIZE),a.precision) as PRECISION,");
        mSql.append("NVL2(t.TYPE_NAME,decode(a.size,0,a.precision,a.size),NULL) as LENGTH,");
        mSql.append("NVL2(t.TYPE_NAME,a.scale,NULL) as SCALE,");
        mSql.append("t.NUM_PREC_RADIX as RADIX,");
        mSql.append("NVL2(t.TYPE_NAME,2,1) as NULLABLE,");
        mSql.append("'' as REMARKS,");
        mSql.append("a.default_val as COLUMN_DEF,");
        mSql.append("t.SQL_DATA_TYPE as SQL_DATA_TYPE,");
        mSql.append("t.SQL_DATETIME_SUB as SQL_DATETIME_SUB,");
        mSql.append("NVL2(t.TYPE_NAME,decode(a.size,0,a.precision,a.size),NULL) as CHAR_OCTET_LENGTH,");
        mSql.append("a.para_order as ORDINAL_POSITION,");
        mSql.append("'' as IS_NULLABLE");
        mSql.append(" FROM (SELECT DB_NAME as PROCEDURE_CAT FROM V$DATABASE) db,");
        mSql.append("system_.sys_proc_paras_ a");
        mSql.append(" LEFT JOIN X$DATATYPE t ON t.data_type=a.data_type,");
        mSql.append("system_.sys_procedures_ b,");
        mSql.append("system_.sys_users_ d");
        mSql.append(" WHERE a.proc_oid=b.proc_oid AND b.user_id=d.user_id");
        mSql.append(aCatalog);
        mSql.append(aSchemaPattern);
        mSql.append(aProcedureNamePattern);
        mSql.append(aColumnNamePattern);
        mSql.append(" ORDER BY PROCEDURE_SCHEM,PROCEDURE_NAME");
        mSql.append(",ORDINAL_POSITION"); // ��� spec�� �ִ°� �ƴѵ�, ���Ǹ� ���� �߰�

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }
    
    public String getProcedureTerm() throws SQLException
    {
    	return "stored procedure";
    }

    public synchronized ResultSet getProcedures(String aCatalog, String aSchemaPattern, String aProcedureNamePattern) throws SQLException
    {
        mSql.setLength(0);
        mSql.append("select db.PROCEDURE_CAT,d.user_name as PROCEDURE_SCHEM, a.proc_name as PROCEDURE_NAME,");
        mSql.append("(select count(*) from system_.sys_proc_paras_ p where ( p.INOUT_TYPE = 0 or p.INOUT_TYPE = 2) AND p.proc_oid = a.proc_oid ) as NUM_INPUT_PARAMS,");
        mSql.append("(select count(*)  from system_.sys_proc_paras_ p where ( p.INOUT_TYPE = 1 or p.INOUT_TYPE = 2) AND p.proc_oid = a.proc_oid ) as NUM_OUTPUT_PARAMS,");
        mSql.append("0  as NUM_RESULT_SETS,");
        mSql.append("'' as REMARKS,decode(a.object_type,0,1,1,2,0) as PROCEDURE_TYPE");
        mSql.append(" from (SELECT DB_NAME as PROCEDURE_CAT FROM V$DATABASE) db,system_.sys_procedures_ a,system_.sys_users_ d");
        mSql.append(" where a.user_id=d.user_id");
        if (!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" AND db.PROCEDURE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aSchemaPattern))
        {
            mSql.append(" AND d.user_name LIKE '");
            mSql.append(aSchemaPattern);
            mSql.append("' escape '\\'");
        }
        if (!StringUtils.isEmpty(aProcedureNamePattern))
        {
            mSql.append(" AND a.proc_name LIKE '");
            mSql.append(aProcedureNamePattern);
            mSql.append("' escape '\\'");
        }
        mSql.append(" ORDER BY PROCEDURE_SCHEM, PROCEDURE_NAME");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        
        return rs;
    }

    public int getResultSetHoldability() throws SQLException
    {
        return AltibaseStatement.DEFAULT_CURSOR_HOLDABILITY;
    }

    public String getSQLKeywords() throws SQLException
    {
    	return "ADD,ALTER,COLUMN,DATE,DROP,FOREGIN,RENAME,IDENTIFIED,INDEX,NUMBER,REPLECATION,BROWSE,BULK,CHECKPOINT,DATABASE,DISK,DUMMY,DUMP,IF,INDEX,LOAD,OFF,PLAN,PRINT,READ,RETURN,ROWCOUNT,RULE,SAVE,STATISTICS,TRIGGER,TRUNCATE";
    }

    public int getSQLStateType() throws SQLException
    {
        return java.sql.DatabaseMetaData.sqlStateXOpen;
    }

    public String getSchemaTerm() throws SQLException
    {
    	return "schema";
    }

    public synchronized ResultSet getSchemas() throws SQLException
    {
        mSql.setLength(0);
        mSql.append("select user_name as TABLE_SCHEM, (SELECT DB_NAME FROM V$DATABASE LIMIT 1) as TABLE_CATALOG");
        mSql.append(" from system_.sys_users_");
        mSql.append(" where system_.sys_users_.user_type = 'U'");  // BUG-45071 username�� ���Խ�Ų��.
        mSql.append(" order by TABLE_SCHEM");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public String getSearchStringEscape() throws SQLException
    {
        return "\\";
    }

    public String getStringFunctions() throws SQLException
    {
        return "ASCII,CASE2,CHOSUNG,CHR,DESDECRYPT,DESENCRYPT,DIGEST,DIGITS,DUMP,INSTR,INSTRB,LENGTH,LPAD,LTRIM,NOTLIKE,REPLACE2,REPLICATE,REVERSE_STR,RPAD,RTRIM,SUB2,SUBSTR,SUBSTRB,TO_CHAR,TO_HEX,TRANSLATE,TRIM,TRUNC,UPPER,CHAR_LENGTH,CHARACTER_LENGTH,LOWER,OCTET_LENGTH,TO_CHAR,TO_NUMBER,TO_DATE,LTRIM,RTRIM,POSITION";
    }

    public synchronized ResultSet getSuperTables(String aCatalog, String aSchemaPattern, String aTableNamePattern) throws SQLException
    {
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery("SELECT '' TABLE_CAT,'' TABLE_SCHEM,'' TABLE_NAME,'' SUPERTABLE_NAME from dual WHERE 0!=0");
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public synchronized ResultSet getSuperTypes(String aCatalog, String aSchemaPattern, String aTypeNamePattern) throws SQLException
    {
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
    	ResultSet rs = stmt.executeQuery("SELECT '' TYPE_CAT,'' TYPE_SCHEM,'' TYPE_NAME,'' SUPERTYPE_CAT,'' SUPERTYPE_SCHEM,'' SUPERTYPE_NAME from dual WHERE 0!=0");
    	((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public String getSystemFunctions() throws SQLException
    {
    	return "DUMP,NVL,DECODE";
    }

    public synchronized ResultSet getTablePrivileges(String aCatalog, String aSchemaPattern, String aTableNamePattern) throws SQLException            
    {
        mSql.setLength(0);
        mSql.append("select db.TABLE_CAT,u_t.user_name TABLE_SCHEM,u_t.table_name TABLE_NAME,u.user_name GRANTOR,u2.user_name GRANTEE,priv.priv_name PRIVILEGE,decode(obj.with_grant_option,1,'YES',0,'NO') IS_GRANTABLE");
        mSql.append(" from (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,");
        mSql.append("(select t.table_id table_id, t.table_name table_name, u.user_name user_name from system_.sys_users_ u, system_.sys_tables_ t where u.user_id = t.user_id and t.table_type=\'T\'");
        if(!StringUtils.isEmpty(aSchemaPattern))
        {
            mSql.append(" and u.user_name LIKE '");
            mSql.append(aSchemaPattern);
            mSql.append("' escape '\\'");
        }
        if(!StringUtils.isEmpty(aTableNamePattern))
        {
            mSql.append(" and t.table_name LIKE '");
            mSql.append(aTableNamePattern);
            mSql.append("' escape '\\'");
        }
        mSql.append(") U_T,");
        mSql.append("system_.sys_users_ u,");
        mSql.append("system_.sys_users_ u2,");
        mSql.append("system_.sys_privileges_ priv,");
        mSql.append("system_.sys_grant_object_ obj");
        mSql.append(" where obj.obj_id = u_t.table_id");
        mSql.append(" and u.user_id = obj.grantor_id");
        mSql.append(" and u2.user_id = obj.grantee_id");
        mSql.append(" and priv.priv_id = obj.priv_id");
        if(!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" and db.TABLE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        mSql.append(" order by TABLE_SCHEM, TABLE_NAME, PRIVILEGE");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    // BUGBUG (2013-01-25) SYNONYM�� �����Ϸ��� ������ ������ �ʿ��ϴ�. (by �輺��)
    // ������ ������ SYNONYM������ �𸣰� �Ͼ�, �� Ÿ�Կ� ���� ��Ÿ�� �� ������ �ش� ��ü�� ã�� �� �ִ�.
    // �Դٰ� SYNONYM�� SYNONYM�� �����ϱ� ������ LOOP�� ���� ���� ó���� �ؾ��ϰ�..
    // �̷� ���� ������ Ŭ���̾�Ʈ���� SYNONYM ���� �������°� �����ϱ�� ��ƴ�.
    // getTables() �� �ƴ϶� getColumns()������ SYNONYM�� �������� �ʴ´�.
    private static final String[] TABLE_TYPES = {
        "TABLE",
        "SYSTEM TABLE",
        "VIEW",
        "SYSTEM VIEW",
        "MATERIALIZED VIEW",
        "QUEUE",
//        "SYNONYM",
        "SEQUENCE",
    };
    private static final String TABLE_TYPES_STRING;
    private static final java.util.Set TABLE_TYPES_SET = new java.util.HashSet();
    static
    {
        StringBuffer sStrBuf = new StringBuffer();
        for (int i = 0; i < TABLE_TYPES.length; i++)
        {
            if (i > 0)
            {
                sStrBuf.append(" | ");
            }
            sStrBuf.append(TABLE_TYPES[i].toUpperCase());
            TABLE_TYPES_SET.add(TABLE_TYPES[i].toUpperCase());
        }
        TABLE_TYPES_STRING = sStrBuf.toString();
    }

    private void checkTableTypes(String[] aTypes) throws SQLException
    {
        if ((aTypes == null) || (aTypes.length == 0))
        {
            return;
        }
        for (int i = 0; i < aTypes.length; i++)
        {
            if (!TABLE_TYPES_SET.contains(aTypes[i].toUpperCase()))
            {
                Error.throwSQLException(ErrorDef.INVALID_ARGUMENT, "Table type", TABLE_TYPES_STRING, aTypes[i]);
            }
        }
    }

    public synchronized ResultSet getTableTypes() throws SQLException
    {
        mSql.setLength(0);
        for (int i = 0; i < TABLE_TYPES.length; i++)
        {
            if (i > 0)
            {
                mSql.append(" UNION ");
            }
            mSql.append("select '");
            mSql.append(TABLE_TYPES[i]);
            mSql.append("' as TABLE_TYPE from dual");
        }
        mSql.append(" order by TABLE_TYPE");
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
    	ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public synchronized ResultSet getTables(String aCatalog, String aSchemaPattern, String aTableNamePattern, String[] aTypes) throws SQLException
    {
        checkTableTypes(aTypes);

        mSql.setLength(0);
        mSql.append("select db.TABLE_CAT");
        mSql.append(",a.user_name  as TABLE_SCHEM");
        mSql.append(",b.table_name as TABLE_NAME");
        mSql.append(",decode(a.user_name,'SYSTEM_','SYSTEM ','') || decode(b.table_type,'T','TABLE','V','VIEW','S','SEQUENCE','Q','QUEUE','M','MATERIALIZED VIEW') as TABLE_TYPE");
        mSql.append(",'' as REMARKS");
        mSql.append(",'' as TYPE_CAT");
        mSql.append(",'' as TYPE_SCHEM");
        mSql.append(",'' as TYPE_NAME");
        mSql.append(",'' as SELF_REFERENCING_COL_NAME");
        mSql.append(",decode(a.user_id,1,'SYSTEM','USER') as REF_GENERATION");
        mSql.append(",b.column_count"); // added by bethy : for PRJ-3344
        mSql.append(" from (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,system_.sys_users_ a,system_.sys_tables_ b");
        mSql.append(" where a.user_id=b.user_id");
        if (!StringUtils.isEmpty(aCatalog))
        {
            mSql.append(" and db.TABLE_CAT='");
            mSql.append(aCatalog);
            mSql.append('\'');
        }
        if (!StringUtils.isEmpty(aSchemaPattern))
        {
            mSql.append(" and a.user_name LIKE '");
            mSql.append(aSchemaPattern);
            mSql.append("' escape '\\'");
        }
        if (!StringUtils.isEmpty(aTableNamePattern))
        {
            mSql.append(" and b.table_name LIKE '");
            mSql.append(aTableNamePattern);
            mSql.append("' escape '\\'");
        }
//        boolean sNeedGetSynonym = false;
        if ((aTypes == null) || (aTypes.length == 0))
        {
            // ��� ����� ���� ��, non-visible�� ��ü�� �ɷ����� �Ѵ�.
            mSql.append(" and (b.table_type IN ('T','V') OR (a.user_name<>'SYSTEM_' AND b.table_type IN ('S','M','Q')))");
//            sNeedGetSynonym = true;
        }
        else
        {
            boolean sIsAdded = false;
            for (int i = 0; i < aTypes.length; i++)
            {
                int sTypeCmpStartIdx = 0;
                String sCondSys = null;
                String sCondType = null;
                if (StringUtils.startsWithIgnoreCase(aTypes[i], "SYSTEM "))
                {
                    sCondSys = "a.user_name='SYSTEM_'";
                    sTypeCmpStartIdx = 7;
                }
                else
                {
                    sCondSys = "a.user_name<>'SYSTEM_'";
                }
                if (StringUtils.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "TABLE") == 0)
                {
                    sCondType = "b.table_type='T'";
                }
                else if (StringUtils.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "SEQUENCE") == 0)
                {
                    sCondType = "b.table_type='S'";
                }
                else if (StringUtils.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "MATERIALIZED VIEW") == 0)
                {
                    sCondType = "b.table_type='M'";
                }
                else if (StringUtils.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "VIEW") == 0)
                {
                    sCondType = "b.table_type='V'";
                }
                else if (StringUtils.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "QUEUE") == 0)
                {
                    sCondType = "b.table_type='Q'";
                }
//                else if (StringUtil.compareIgnoreCase(aTypes[i], sTypeCmpStartIdx, "SYNONYM") == 0)
//                {
//                    // SYS_TABLES_ ���� SYS_SYNONYMS_���� ��ȸ�ؾ� �ϹǷ� flag�� �����صд�.
//                    sNeedGetSynonym = true;
//                }
                else
                {
                    Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                }
                if (sCondType != null)
                {
                    if (!sIsAdded)
                    {
                        mSql.append(" and ((");
                        mSql.append(sCondType);
                        sIsAdded = true;
                    }
                    else
                    {
                        mSql.append(" or (");
                        mSql.append(sCondType);
                    }
                    if (sCondSys != null)
                    {
                        mSql.append(" and ");
                        mSql.append(sCondSys);
                    }
                    mSql.append(')');
                }
            }
            if (sIsAdded)
            {
                mSql.append(')');
            }
        }
//        if (sNeedGetSynonym == true)
//        {
//            // SYNONYM�� ��ȸ�� ���
//            if (aTypes != null && aTypes.length == 1)
//            {
//                mSql.setLength(0);
//            }
//            else
//            {
//                mSql.append(" UNION ");
//            }
//            mSql.append("select db.TABLE_CAT");
//            mSql.append(",a.user_name  as TABLE_SCHEM");
//            mSql.append(",b.synonym_name as TABLE_NAME");
//            mSql.append(",decode(a.user_name,'SYSTEM_','SYSTEM SYNONYM','SYNONYM') as TABLE_TYPE");
//            mSql.append(",'' as REMARKS");
//            mSql.append(",'' as TYPE_CAT");
//            mSql.append(",'' as TYPE_SCHEM");
//            mSql.append(",'' as TYPE_NAME");
//            mSql.append(",'' as SELF_REFERENCING_COL_NAME");
//            mSql.append(",decode(a.user_id,1,'SYSTEM','USER') as REF_GENERATION");
//            mSql.append(",0 as column_count"); // added by bethy : for PRJ-3344
//            mSql.append(" from (SELECT DB_NAME as TABLE_CAT FROM V$DATABASE) db,SYSTEM_.SYS_SYNONYMS_ b left outer join system_.sys_users_ a on a.USER_ID = b.SYNONYM_OWNER_ID");
//            mSql.append(" where b.synonym_name is not null");
//            if (!StringUtil.isEmpty(aCatalog))
//            {
//                mSql.append(" and db.TABLE_CAT='");
//                mSql.append(aCatalog);
//                mSql.append('\'');
//            }
//            if (!StringUtil.isEmpty(aSchemaPattern))
//            {
//                mSql.append(" and a.user_name LIKE '");
//                mSql.append(aSchemaPattern);
//                mSql.append("' escape '\\'");
//            }
//            if (!StringUtil.isEmpty(aTableNamePattern))
//            {
//                mSql.append(" and b.synonym_name LIKE '");
//                mSql.append(aTableNamePattern);
//                mSql.append("' escape '\\'");
//            }
//        }
        mSql.append(" order by TABLE_TYPE, TABLE_SCHEM, TABLE_NAME");

        Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public String getTimeDateFunctions() throws SQLException
    {
        return "SYSDATE, TRUNC, EXTRACT, LAST_DAY, ADD_MONTHS, NEXT_DAY";
    }

    public synchronized ResultSet getTypeInfo() throws SQLException
    {
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery("select TYPE_NAME, decode(DATA_TYPE,-8,-15,9,93,30,2004,40,2005,DATA_TYPE) DATA_TYPE, FIXED_PREC_SCALE as PRECISION, LITERAL_PREFIX, LITERAL_SUFFIX, CREATE_PARAM as CREATE_PARAMS, NULLABLE, CASE_SENSITIVE, SEARCHABLE, UNSIGNED_ATTRIBUTE, FIXED_PREC_SCALE, 'F' as AUTO_INCREMENT, LOCAL_TYPE_NAME, MINIMUM_SCALE, MAXIMUM_SCALE, SQL_DATA_TYPE, SQL_DATETIME_SUB, NUM_PREC_RADIX from V$DATATYPE where DATA_TYPE != 60 and DATA_TYPE != 61 order by DATA_TYPE");
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }

    public ResultSet getUDTs(String aCatalog, String aSchemaPattern, String aTypeNamePattern, int[] aTypes) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "User defined type");
        return null;
    }

    public String getURL() throws SQLException
    {
        return mConn.getURL();
    }

    public String getUserName() throws SQLException
    {
        String sUserName = mConn.getUserName();
        return (sUserName == null) ? "" : sUserName;
    }

    public ResultSet getVersionColumns(String catalog, String schema, String table) throws SQLException
    {
    	AltibaseResultSet // !# Not yet support
        rs = (AltibaseResultSet)getBestRowIdentifier(catalog, schema, table, 0, true);
        return rs;
    }

    public boolean insertsAreDetected(int type) throws SQLException
    {
    	return false;
    }

    public boolean isCatalogAtStart() throws SQLException
    {
    	return false;
    }

    public boolean isReadOnly() throws SQLException
    {
    	return false;
    }

    public boolean locatorsUpdateCopy() throws SQLException
    {
    	return true;
    }

    public boolean nullPlusNonNullIsNull() throws SQLException
    {
    	return true;
    }

    public boolean nullsAreSortedAtEnd() throws SQLException
    {
    	return false;
    }

    public boolean nullsAreSortedAtStart() throws SQLException
    {
    	return false;
    }

    public boolean nullsAreSortedHigh() throws SQLException
    {
    	return false;
    }

    public boolean nullsAreSortedLow() throws SQLException
    {
    	return true;
    }

    public boolean othersDeletesAreVisible(int type) throws SQLException
    {
    	if(type == ResultSet.TYPE_SCROLL_SENSITIVE) 
        {
        	return true;
        }
        else
        {
        	return false;
        }
    }

    public boolean othersInsertsAreVisible(int type) throws SQLException
    {
        return false;
    }

    public boolean othersUpdatesAreVisible(int type) throws SQLException
    {
    	if(type == ResultSet.TYPE_SCROLL_SENSITIVE) 
        {
        	return true;
        }
        else
        {
        	return false;
        }
    }

    public boolean ownDeletesAreVisible(int type) throws SQLException
    {
    	if(type == ResultSet.TYPE_SCROLL_SENSITIVE) 
        {
        	return true;
        }
        else
        {
        	return false;
        }
    }

    public boolean ownInsertsAreVisible(int type) throws SQLException
    {
        return false;
    }

    public boolean ownUpdatesAreVisible(int type) throws SQLException
    {
    	if(type == ResultSet.TYPE_SCROLL_SENSITIVE) 
        {
        	return true;
        }
        else
        {
        	return false;
        }
    }

    public boolean storesLowerCaseIdentifiers() throws SQLException
    {
    	return false;
    }

    public boolean storesLowerCaseQuotedIdentifiers() throws SQLException
    {
    	return false;
    }

    public boolean storesMixedCaseIdentifiers() throws SQLException
    {
    	return false;
    }

    public boolean storesMixedCaseQuotedIdentifiers() throws SQLException
    {
    	return true;
    }

    public boolean storesUpperCaseIdentifiers() throws SQLException
    {
    	return true;
    }

    public boolean storesUpperCaseQuotedIdentifiers() throws SQLException
    {
        return false;
    }

    public boolean supportsANSI92EntryLevelSQL() throws SQLException
    {
    	return true;
    }

    public boolean supportsANSI92FullSQL() throws SQLException
    {
    	return true;
    }

    public boolean supportsANSI92IntermediateSQL() throws SQLException
    {
    	return true;
    }

    public boolean supportsAlterTableWithAddColumn() throws SQLException
    {
        return true;
    }

    public boolean supportsAlterTableWithDropColumn() throws SQLException
    {
        return true;
    }

    public boolean supportsBatchUpdates() throws SQLException
    {
        return true;
    }

    public boolean supportsCatalogsInDataManipulation() throws SQLException
    {
        return false;
    }

    public boolean supportsCatalogsInIndexDefinitions() throws SQLException
    {
        return false;
    }

    public boolean supportsCatalogsInPrivilegeDefinitions() throws SQLException
    {
        return false;
    }

    public boolean supportsCatalogsInProcedureCalls() throws SQLException
    {
        return false;
    }

    public boolean supportsCatalogsInTableDefinitions() throws SQLException
    {
        return false;
    }

    public boolean supportsColumnAliasing() throws SQLException
    {
    	return true;
    }

    public boolean supportsConvert() throws SQLException
    {
        return false;
    }

    public boolean supportsConvert(int fromType, int toType) throws SQLException
    {
        return false;
    }

    public boolean supportsCoreSQLGrammar() throws SQLException
    {
    	return true;
    }

    public boolean supportsCorrelatedSubqueries() throws SQLException
    {
    	return true;
    }

    public boolean supportsDataDefinitionAndDataManipulationTransactions() throws SQLException
    {
        return true;
    }

    public boolean supportsDataManipulationTransactionsOnly() throws SQLException
    {
        return false;
    }

    public boolean supportsDifferentTableCorrelationNames() throws SQLException
    {
        return false;
    }

    public boolean supportsExpressionsInOrderBy() throws SQLException
    {
    	return true;
    }

    public boolean supportsExtendedSQLGrammar() throws SQLException
    {
	    return true;
    }

    public boolean supportsFullOuterJoins() throws SQLException
    {
    	return true;
    }

    public boolean supportsGetGeneratedKeys() throws SQLException
    {
        return true;
    }

    public boolean supportsGroupBy() throws SQLException
    {
        return true;
    }

    public boolean supportsGroupByBeyondSelect() throws SQLException
    {
    	return true;
    }

    public boolean supportsGroupByUnrelated() throws SQLException
    {
        return false;
    }

    public boolean supportsIntegrityEnhancementFacility() throws SQLException
    {
    	return true;
    }

    public boolean supportsLikeEscapeClause() throws SQLException
    {
    	return true;
    }

    public boolean supportsLimitedOuterJoins() throws SQLException
    {
        return true;
    }

    public boolean supportsMinimumSQLGrammar() throws SQLException
    {
        return true;
    }

    public boolean supportsMixedCaseIdentifiers() throws SQLException
    {
    	return false;
    }

    public boolean supportsMixedCaseQuotedIdentifiers() throws SQLException
    {
    	return true;
    }

    public boolean supportsMultipleOpenResults() throws SQLException
    {
    	return true;
    }

    public boolean supportsMultipleResultSets() throws SQLException
    {
        return false;
    }

    public boolean supportsMultipleTransactions() throws SQLException
    {
        return false;
    }

    public boolean supportsNamedParameters() throws SQLException
    {
        return false;
    }

    public boolean supportsNonNullableColumns() throws SQLException
    {
    	return true;
    }

    public boolean supportsOpenCursorsAcrossCommit() throws SQLException
    {
        return true;
    }

    public boolean supportsOpenCursorsAcrossRollback() throws SQLException
    {
        return true;
    }

    public boolean supportsOpenStatementsAcrossCommit() throws SQLException
    {
        return true;
    }

    public boolean supportsOpenStatementsAcrossRollback() throws SQLException
    {
        return true;
    }

    public boolean supportsOrderByUnrelated() throws SQLException
    {
        return true;
    }

    public boolean supportsOuterJoins() throws SQLException
    {
        return true;
    }

    public boolean supportsPositionedDelete() throws SQLException
    {
        return false;	// Websphere 5 - confused by try to use that 'true'
    }

    public boolean supportsPositionedUpdate() throws SQLException
    {
        return false;	// Websphere 5 - confused by try to use that 'true'
    }

    public boolean supportsResultSetConcurrency(int type, int concur) throws SQLException
    {
        boolean ret = false;
        switch (type)
        {
            case ResultSet.TYPE_SCROLL_SENSITIVE:
            case ResultSet.TYPE_SCROLL_INSENSITIVE:
            case ResultSet.TYPE_FORWARD_ONLY:
                ret = (concur == ResultSet.CONCUR_READ_ONLY) ||
                      (concur == ResultSet.CONCUR_UPDATABLE);
            default:
                return ret;
        }
    }

    public boolean supportsResultSetHoldability(int holdability) throws SQLException
    {
        return ((holdability == ResultSet.CLOSE_CURSORS_AT_COMMIT) || (holdability == ResultSet.HOLD_CURSORS_OVER_COMMIT));
    }

    public boolean supportsResultSetType(int type) throws SQLException
    {
        return ((type == ResultSet.TYPE_FORWARD_ONLY) || (type == ResultSet.TYPE_SCROLL_INSENSITIVE) || (type == ResultSet.TYPE_SCROLL_SENSITIVE));
    }

    public boolean supportsSavepoints() throws SQLException
    {
        return true;
    }

    public boolean supportsSchemasInDataManipulation() throws SQLException
    {
        return true;
    }

    public boolean supportsSchemasInIndexDefinitions() throws SQLException
    {
        return true;
    }

    public boolean supportsSchemasInPrivilegeDefinitions() throws SQLException
    {
        return true;
    }

    public boolean supportsSchemasInProcedureCalls() throws SQLException
    {
        return true;
    }

    public boolean supportsSchemasInTableDefinitions() throws SQLException
    {
        return true;
    }

    public boolean supportsSelectForUpdate() throws SQLException
    {
        return false;
    }

    public boolean supportsStatementPooling() throws SQLException
    {
        return false;
    }

    public boolean supportsStoredProcedures() throws SQLException
    {
        return true;
    }

    public boolean supportsSubqueriesInComparisons() throws SQLException
    {
        return true;
    }

    public boolean supportsSubqueriesInExists() throws SQLException
    {
        return true;
    }

    public boolean supportsSubqueriesInIns() throws SQLException
    {
        return true;
    }

    public boolean supportsSubqueriesInQuantifieds() throws SQLException
    {
        return true;
    }

    public boolean supportsTableCorrelationNames() throws SQLException
    {
        return true;
    }

    public boolean supportsTransactionIsolationLevel(int level) throws SQLException
    {
    	boolean ret = false;
        switch(level)
        {
            case Connection.TRANSACTION_READ_COMMITTED:
            case Connection.TRANSACTION_REPEATABLE_READ:
            case Connection.TRANSACTION_SERIALIZABLE:
                ret = true;
            default:
                return ret;
        }
    }

    public boolean supportsTransactions() throws SQLException
    {
        return true;
    }

    public boolean supportsUnion() throws SQLException
    {
        return true;
    }

    public boolean supportsUnionAll() throws SQLException
    {
        return true;
    }

    public boolean updatesAreDetected(int type) throws SQLException
    {
        return false;
    }

    public boolean usesLocalFilePerTable() throws SQLException
    {
        return false;
    }

    public boolean usesLocalFiles() throws SQLException
    {
        return false;
    }
    
    private ResultSet keys_query(String aPrimaryCatalog, String aPrimarySchema, String aPrimaryTable, String aForeignCatalog, String aForeignSchema, String aForeignTable, String aOrderByClause) throws SQLException
    {
        mSql.setLength(0);
        mSql.append("SELECT P.PKTABLE_CAT as PKTABLE_CAT");
        mSql.append(",P.PKTABLE_SCHEM as PKTABLE_SCHEM");
        mSql.append(",P.PKTABLE_NAME as PKTABLE_NAME");
        mSql.append(",P.PK_COLUMN_NAME as PKCOLUMN_NAME");
        mSql.append(",F.FKTABLE_CAT as FKTABLE_CAT");
        mSql.append(",F.FKTABLE_SCHEM as FKTABLE_SCHEM");
        mSql.append(",F.FKTABLE_NAME as FKTABLE_NAME");
        mSql.append(",F.FK_COLUMN_NAME as FKCOLUMN_NAME");
        mSql.append(",F.KEY_SEQ as KEY_SEQ");
        mSql.append(",3 as UPDATE_RULE");
        mSql.append(",3 as DELETE_RULE");
        mSql.append(",F.FK_NAME as FK_NAME");
        mSql.append(",P.PK_NAME as PK_NAME");
        mSql.append(",7 as DEFERRABILITY");
        mSql.append(" FROM");
        /* -------- PRIMARY/IMPORT SOURCE ------------- */
        mSql.append("(SELECT pdb.PKTABLE_CAT");
        mSql.append(",pe.user_name as PKTABLE_SCHEM");
        mSql.append(",pd.table_name as PKTABLE_NAME");
        mSql.append(",pc.column_name as PK_COLUMN_NAME");
        mSql.append(",pa.constraint_name as PK_NAME");
        mSql.append(",pb.constraint_col_order+1 as KEY_SEQ");
        mSql.append(",pd.table_id as CID");
        mSql.append(",pa.INDEX_ID as idxid");
        mSql.append(" FROM (SELECT DB_NAME as PKTABLE_CAT FROM V$DATABASE) pdb");
        mSql.append(",system_.sys_constraints_ pa");
        mSql.append(",system_.sys_constraint_columns_ pb");
        mSql.append(",system_.sys_columns_ pc");
        mSql.append(",system_.sys_tables_ pd");
        mSql.append(",system_.sys_users_ pe");
        mSql.append(" WHERE ");
        if (!StringUtils.isEmpty(aPrimaryCatalog))
        {
            mSql.append("pdb.PKTABLE_CAT='");
            mSql.append(aPrimaryCatalog);
            mSql.append("' and ");
        }
        mSql.append("(((");
        if (!StringUtils.isEmpty(aPrimarySchema))
        {
            mSql.append("pe.user_name='");
            mSql.append(aPrimarySchema);
            mSql.append("' and ");
        }
        mSql.append("pe.user_id=pd.user_id)");
        if (!StringUtils.isEmpty(aPrimaryTable))
        {
            mSql.append(" and pd.table_name='");
            mSql.append(aPrimaryTable);
            mSql.append('\'');
        }
        mSql.append(" and pe.user_id=pa.user_id");
        mSql.append(" and pa.table_id=pd.table_id)");
        mSql.append(" and pa.constraint_id=pb.constraint_id");
        mSql.append(" and pa.constraint_type=");
        mSql.append(QCM_FOREIGN_KEY);
        mSql.append(" and pb.column_id=pc.column_id)");
        mSql.append(") AS P");
        /* --------- FOREIGN/EXPORT SOURCE ------------- */
        mSql.append(" INNER JOIN (");
        mSql.append("select fdb.FKTABLE_CAT");
        mSql.append(",fe.user_name as FKTABLE_SCHEM");
        mSql.append(",fd.table_name as FKTABLE_NAME");
        mSql.append(",fc.column_name as FK_COLUMN_NAME");
        mSql.append(",fa.constraint_name as FK_NAME");
        mSql.append(",fb.constraint_col_order+1 as KEY_SEQ");
        mSql.append(",fa.referenced_table_id as CID");
        mSql.append(",fa.REFERENCED_INDEX_ID as idxid");
        mSql.append(" FROM (SELECT DB_NAME as FKTABLE_CAT FROM V$DATABASE) fdb");
        mSql.append(",system_.sys_constraints_ fa");
        mSql.append(",system_.sys_constraint_columns_ fb");
        mSql.append(",system_.sys_columns_ fc");
        mSql.append(",system_.sys_tables_ fd");
        mSql.append(",system_.sys_users_ fe");
        mSql.append(" WHERE ");
        if (!StringUtils.isEmpty(aForeignCatalog))
        {
            mSql.append("fdb.FKTABLE_CAT='");
            mSql.append(aForeignCatalog);
            mSql.append("' and ");
        }
        mSql.append("(((");
        if (!StringUtils.isEmpty(aForeignSchema))
        {
            mSql.append("fe.user_name='");
            mSql.append(aForeignSchema);
            mSql.append("' and ");
        }
        mSql.append("fe.user_id=fd.user_id)");
        if (!StringUtils.isEmpty(aForeignTable))
        {
            mSql.append(" and fd.table_name='");
            mSql.append(aForeignTable);
            mSql.append('\'');
        }
        mSql.append(" and fe.user_id=fa.user_id");
        mSql.append(" and fa.table_id=fd.table_id )");
        mSql.append(" and fa.constraint_id=fb.constraint_id");
        mSql.append(" and fb.column_id=fc.column_id)) AS F");
        mSql.append(" ON (P.CID=F.CID and P.IDXID=F.IDXID and P.KEY_SEQ=F.KEY_SEQ)");
        mSql.append(" ORDER BY ");
        mSql.append(aOrderByClause);
    	Statement stmt = mConn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, ResultSet.HOLD_CURSORS_OVER_COMMIT);
        ResultSet rs = stmt.executeQuery(mSql.toString());
        ((AltibaseResultSet)rs).registerTarget(stmt);
        return rs;
    }    
}
