-- ============================================================================
-- SCHEMA V3 - NOTIONAL AND VALUATION ENHANCEMENTS
-- Created: 2011-03-08
-- Author: K. Nakamura (Quantitative Risk Team)
-- Database: MS SQL Server 2008 R2
-- ============================================================================
-- $Header: /cvsroot/trading/db/schema_v3_notional.sql,v 1.34 2011/11/20 16:30:00 knakamura Exp $
-- $Revision: 1.34 $
-- ============================================================================
--
-- MIGRATION NOTES:
-- This migration adds notional amount tracking and valuation fields
-- required for Dodd-Frank compliance.
--
-- PERFORMANCE WARNING: This adds columns to TRD_BLOTTER which has
-- ~50M rows. Run during weekend maintenance window with minimal blocking.
--
-- Estimated runtime: 4-6 hours on production
-- ============================================================================

-- Use WITH (ONLINE = ON) for SQL Server 2008 Enterprise Edition
-- #define ONLINE_INDEX_OPTION WITH (ONLINE = ON)

BEGIN TRANSACTION
GO

PRINT 'Starting Schema V3 Migration - Notional Enhancements'
PRINT GETDATE()
PRINT 'WARNING: This migration may take 4-6 hours on large tables'
GO

-- ============================================================================
-- Add notional amount columns to trade blotter
-- ============================================================================
-- NOTE: Using DECIMAL(22,2) for notional to handle large currency amounts
-- Some emerging market trades have notional > 1 trillion
-- ============================================================================

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'NTNL_AMT')
BEGIN
    -- Add column as NULL first, populate data, then add DEFAULT
    ALTER TABLE TRD_BLOTTER ADD NTNL_AMT DECIMAL(22,2) NULL
    PRINT 'Added NTNL_AMT column (nullable for data migration)'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'NTNL_CCY_CD')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD NTNL_CCY_CD CHAR(3) NULL
    PRINT 'Added NTNL_CCY_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'NTNL_AMT_USD')
BEGIN
    -- USD equivalent for reporting
    ALTER TABLE TRD_BLOTTER ADD NTNL_AMT_USD DECIMAL(22,2) NULL
    PRINT 'Added NTNL_AMT_USD column'
END
GO

-- ============================================================================
-- Data migration for existing trades
-- ============================================================================
-- IMPORTANT: Run this in batches to avoid blocking
-- ============================================================================
-- DECLARE @BatchSize INT = 50000
-- DECLARE @RowsAffected INT = 1
-- 
-- WHILE @RowsAffected > 0
-- BEGIN
--     UPDATE TOP (@BatchSize) TRD_BLOTTER
--     SET NTNL_CCY_CD = CCY_CD,
--         NTNL_AMT = 0,  -- Historical trades have notional in separate system
--         NTNL_AMT_USD = 0
--     WHERE NTNL_AMT IS NULL
--     
--     SET @RowsAffected = @@ROWCOUNT
--     PRINT 'Migrated ' + CAST(@RowsAffected AS VARCHAR) + ' rows'
--     WAITFOR DELAY '00:00:01'  -- Pause to reduce blocking
-- END
-- GO

-- After data migration, add default constraints
-- ALTER TABLE TRD_BLOTTER ADD CONSTRAINT DF_TRD_BLOTTER_NTNL_AMT 
--     DEFAULT 0 FOR NTNL_AMT
-- ALTER TABLE TRD_BLOTTER ADD CONSTRAINT DF_TRD_BLOTTER_NTNL_CCY_CD 
--     DEFAULT 'USD' FOR NTNL_CCY_CD
-- GO

-- ============================================================================
-- Add valuation columns
-- ============================================================================
IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'MTM_VAL')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD MTM_VAL DECIMAL(18,6) NULL
    PRINT 'Added MTM_VAL column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'MTM_CCY_CD')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD MTM_CCY_CD CHAR(3) NULL
    PRINT 'Added MTM_CCY_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'MTM_VAL_DT')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD MTM_VAL_DT DATETIME NULL
    PRINT 'Added MTM_VAL_DT column'
END
GO

-- ============================================================================
-- Add leg information for swaps
-- ============================================================================
IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'PAY_FIXED_FLG')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD PAY_FIXED_FLG CHAR(1) NULL
    PRINT 'Added PAY_FIXED_FLG column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'FIXED_RATE')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD FIXED_RATE DECIMAL(12,8) NULL
    PRINT 'Added FIXED_RATE column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'FLT_IDX_CD')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD FLT_IDX_CD VARCHAR(8) NULL
    PRINT 'Added FLT_IDX_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'FLT_SPREAD')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD FLT_SPREAD DECIMAL(12,8) NULL
    PRINT 'Added FLT_SPREAD column'
END
GO

-- ============================================================================
-- Create trade leg detail table (for multi-leg trades)
-- ============================================================================
-- NOTE: This replaces the deprecated TRD_LEG_DETAIL_OLD table
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'TRD_LEG_DTL' AND type = 'U')
    DROP TABLE TRD_LEG_DTL
GO

CREATE TABLE TRD_LEG_DTL (
    TRD_LEG_ID       INT IDENTITY(1,1) NOT NULL,
    TRD_ID           INT NOT NULL,
    LEG_NUM          TINYINT NOT NULL,
    LEG_DIR_CD       CHAR(1) NOT NULL,  -- P=Pay, R=Receive
    LEG_TYP_CD       VARCHAR(8) NOT NULL, -- FIXED, FLOATING, etc.
    NTNL_AMT         DECIMAL(22,2) NOT NULL,
    NTNL_CCY_CD      CHAR(3) NOT NULL,
    FIXED_RATE       DECIMAL(12,8) NULL,
    FLT_IDX_CD       VARCHAR(8) NULL,
    FLT_SPREAD       DECIMAL(12,8) NULL,
    PAY_FREQ_CD      VARCHAR(8) NULL,
    DAY_CNT_CD       VARCHAR(8) NULL,
    EFF_DT           DATETIME NOT NULL,
    MAT_DT           DATETIME NOT NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_TRD_LEG_DTL PRIMARY KEY (TRD_LEG_ID),
    CONSTRAINT FK_TRD_LEG_DTL_TRD FOREIGN KEY (TRD_ID) 
        REFERENCES TRD_BLOTTER(TRD_ID),
    CONSTRAINT UK_TRD_LEG_DTL UNIQUE (TRD_ID, LEG_NUM)
)
GO

CREATE INDEX IDX_TRD_LEG_DTL_TRD ON TRD_LEG_DTL(TRD_ID)
GO

-- ============================================================================
-- Day count convention reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'DAY_CNT_REF' AND type = 'U')
    DROP TABLE DAY_CNT_REF
GO

CREATE TABLE DAY_CNT_REF (
    DAY_CNT_CD       VARCHAR(8) NOT NULL,
    DAY_CNT_NM       VARCHAR(32) NOT NULL,
    DAY_CNT_DESC     VARCHAR(128) NULL,
    
    CONSTRAINT PK_DAY_CNT_REF PRIMARY KEY (DAY_CNT_CD)
)
GO

INSERT INTO DAY_CNT_REF (DAY_CNT_CD, DAY_CNT_NM, DAY_CNT_DESC) VALUES 
    ('ACT360', 'Actual/360', 'Actual days divided by 360')
INSERT INTO DAY_CNT_REF (DAY_CNT_CD, DAY_CNT_NM, DAY_CNT_DESC) VALUES 
    ('ACT365', 'Actual/365', 'Actual days divided by 365')
INSERT INTO DAY_CNT_REF (DAY_CNT_CD, DAY_CNT_NM, DAY_CNT_DESC) VALUES 
    ('30360', '30/360', '30 days per month, 360 day year')
INSERT INTO DAY_CNT_REF (DAY_CNT_CD, DAY_CNT_NM, DAY_CNT_DESC) VALUES 
    ('ACTACT', 'Actual/Actual', 'Actual days divided by actual days in period')
GO

-- ============================================================================
-- Payment frequency reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'PAY_FREQ_REF' AND type = 'U')
    DROP TABLE PAY_FREQ_REF
GO

CREATE TABLE PAY_FREQ_REF (
    PAY_FREQ_CD      VARCHAR(8) NOT NULL,
    PAY_FREQ_NM      VARCHAR(32) NOT NULL,
    PAY_FREQ_MONTHS  TINYINT NOT NULL,
    
    CONSTRAINT PK_PAY_FREQ_REF PRIMARY KEY (PAY_FREQ_CD)
)
GO

INSERT INTO PAY_FREQ_REF (PAY_FREQ_CD, PAY_FREQ_NM, PAY_FREQ_MONTHS) VALUES 
    ('MNTH', 'Monthly', 1)
INSERT INTO PAY_FREQ_REF (PAY_FREQ_CD, PAY_FREQ_NM, PAY_FREQ_MONTHS) VALUES 
    ('QTR', 'Quarterly', 3)
INSERT INTO PAY_FREQ_REF (PAY_FREQ_CD, PAY_FREQ_NM, PAY_FREQ_MONTHS) VALUES 
    ('SEMI', 'Semi-Annual', 6)
INSERT INTO PAY_FREQ_REF (PAY_FREQ_CD, PAY_FREQ_NM, PAY_FREQ_MONTHS) VALUES 
    ('ANNU', 'Annual', 12)
GO

-- ============================================================================
-- Add floating index reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'FLT_IDX_REF' AND type = 'U')
    DROP TABLE FLT_IDX_REF
GO

CREATE TABLE FLT_IDX_REF (
    FLT_IDX_CD       VARCHAR(8) NOT NULL,
    FLT_IDX_NM       VARCHAR(32) NOT NULL,
    FLT_IDX_TENOR    VARCHAR(8) NULL,
    FLT_IDX_CCY      CHAR(3) NOT NULL,
    
    CONSTRAINT PK_FLT_IDX_REF PRIMARY KEY (FLT_IDX_CD)
)
GO

INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('LIBOR3M', '3M LIBOR', 'USD')
INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('LIBOR6M', '6M LIBOR', 'USD')
INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('EURIB3M', '3M EURIBOR', 'EUR')
INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('EURIB6M', '6M EURIBOR', 'EUR')
INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('TIBOR3M', '3M TIBOR', 'JPY')
INSERT INTO FLT_IDX_REF (FLT_IDX_CD, FLT_IDX_NM, FLT_IDX_CCY) VALUES 
    ('SOFR', 'SOFR', 'USD')
GO

-- ============================================================================
-- Add indexes for new columns
-- ============================================================================
-- NOTE: Creating indexes AFTER data population is more efficient
-- ============================================================================
-- CREATE INDEX IDX_TRD_BLOTTER_NTNL_CCY ON TRD_BLOTTER(NTNL_CCY_CD)
-- GO
-- 
-- CREATE INDEX IDX_TRD_BLOTTER_MTM_DT ON TRD_BLOTTER(MTM_VAL_DT)
-- GO

-- ============================================================================
-- Deprecated columns - DO NOT USE
-- ============================================================================
-- These columns are kept for backwards compatibility with legacy reports
-- Will be removed in V5 migration
-- ============================================================================
-- ALTER TABLE TRD_BLOTTER ADD LEGACY_NOTIONAL_OLD DECIMAL(18,2) NULL
-- ALTER TABLE TRD_BLOTTER ADD LEGACY_RATE_OLD DECIMAL(10,6) NULL
-- GO

PRINT 'Schema V3 Migration Complete'
PRINT GETDATE()
GO

COMMIT TRANSACTION
GO

-- $Log: schema_v3_notional.sql,v $
-- Revision 1.34  2011/11/20 16:30:00  knakamura
-- Added SOFR index reference for LIBOR transition
--
-- Revision 1.33  2011/09/05 10:15:00  knakamura
-- Added floating spread column
--
-- Revision 1.32  2011/06/22 14:00:00  knakamura
-- Fixed notional precision for large amounts
--
-- Revision 1.31  2011/03/08 09:30:00  knakamura
-- Initial notional enhancement migration
-- ============================================================================
